/*
 * File:        block_chain.h
 * Module:      pipeline
 * Purpose:     Connects streaming stages into a chain across threads
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include "ld_creator/pipeline/stage_interfaces.h"

namespace ld_creator {

// Receives a stream of blocks. Consume() is called for each block in order,
// then Close() once. Implementations are used by one thread at a time.
template <typename T>
class IBlockConsumer {
 public:
  virtual ~IBlockConsumer() = default;
  virtual void Consume(T&& block) = 0;
  virtual void Close() = 0;
};

// Returns true for a block that carries nothing and need not be forwarded.
// Edge blocks are always forwarded: an empty edge block still advances the
// completion watermark.
inline bool IsEmptyBlock(const IreSampleBlock& block) {
  return block.ire.empty();
}
inline bool IsEmptyBlock(const CarrierPhaseBlock& block) {
  return block.phase_rad.empty();
}
inline bool IsEmptyBlock(const PhotocurrentBlock& block) {
  return block.level.empty();
}
inline bool IsEmptyBlock(const RfCodeBlock& block) {
  return block.codes.empty();
}
inline bool IsEmptyBlock(const RecordedEdgeBlock& /*block*/) { return false; }
inline bool IsEmptyBlock(const PitEdgeBlock& /*block*/) { return false; }
inline bool IsEmptyBlock(const ScanBlock& /*block*/) { return false; }

// Adapts a stage to the consumer interface: each block is processed by the
// stage and the result passed downstream. Close() finishes the stage, passes
// its remaining output on and closes the downstream consumer.
//
// Ownership: borrows `stage` and `downstream`; both must outlive the node.
template <typename In, typename Out>
class StageNode final : public IBlockConsumer<In> {
 public:
  StageNode(IBlockStage<In, Out>* stage, IBlockConsumer<Out>* downstream)
      : stage_(stage), downstream_(downstream) {
    assert(stage_ != nullptr);
    assert(downstream_ != nullptr);
  }

  void Consume(In&& block) override {
    Out output;
    stage_->Process(block, &output);
    if (!IsEmptyBlock(output)) {
      downstream_->Consume(std::move(output));
    }
  }

  void Close() override {
    Out output;
    stage_->Finish(&output);
    if (!IsEmptyBlock(output)) {
      downstream_->Consume(std::move(output));
    }
    downstream_->Close();
  }

 private:
  IBlockStage<In, Out>* stage_;
  IBlockConsumer<Out>* downstream_;
};

// A thread boundary. Blocks passed to Consume() are queued and delivered to
// `downstream` on a worker thread, in order, so the stages after the handoff
// run concurrently with those before it. Order is preserved, so the output
// is identical with or without the handoff.
//
// An exception thrown downstream is captured on the worker and rethrown from
// the next Consume() or from Close(). Close() joins the worker.
//
// Thread safety: Consume() and Close() are called from one producer thread.
// Ownership: borrows `downstream`, which must outlive the handoff.
template <typename T>
class ThreadedHandoff final : public IBlockConsumer<T> {
 public:
  // `capacity` bounds the queue, which bounds memory use.
  ThreadedHandoff(IBlockConsumer<T>* downstream, std::size_t capacity)
      : downstream_(downstream), capacity_(capacity) {
    assert(downstream_ != nullptr);
    assert(capacity_ > 0);
    worker_ = std::thread([this] { Run(); });
  }

  ThreadedHandoff(const ThreadedHandoff&) = delete;
  ThreadedHandoff& operator=(const ThreadedHandoff&) = delete;

  ~ThreadedHandoff() override {
    if (worker_.joinable()) {
      {
        const std::lock_guard<std::mutex> lock(mutex_);
        abandoned_ = true;
        closing_ = true;
      }
      changed_.notify_all();
      worker_.join();
    }
  }

  void Consume(T&& block) override {
    std::unique_lock<std::mutex> lock(mutex_);
    changed_.wait(lock,
                  [this] { return queue_.size() < capacity_ || failure_; });
    if (failure_) {
      std::rethrow_exception(failure_);
    }
    queue_.push_back(std::move(block));
    lock.unlock();
    changed_.notify_all();
  }

  void Close() override {
    {
      const std::lock_guard<std::mutex> lock(mutex_);
      closing_ = true;
    }
    changed_.notify_all();
    worker_.join();
    if (failure_) {
      std::rethrow_exception(failure_);
    }
  }

 private:
  void Run() {
    try {
      while (true) {
        std::optional<T> block;
        {
          std::unique_lock<std::mutex> lock(mutex_);
          changed_.wait(lock, [this] { return !queue_.empty() || closing_; });
          if (abandoned_) {
            return;
          }
          if (queue_.empty()) {
            break;
          }
          block.emplace(std::move(queue_.front()));
          queue_.pop_front();
        }
        changed_.notify_all();
        downstream_->Consume(std::move(*block));
      }
      downstream_->Close();
    } catch (...) {
      {
        const std::lock_guard<std::mutex> lock(mutex_);
        failure_ = std::current_exception();
      }
      changed_.notify_all();
    }
  }

  IBlockConsumer<T>* downstream_;
  std::size_t capacity_;
  std::mutex mutex_;
  std::condition_variable changed_;
  std::deque<T> queue_;
  bool closing_ = false;
  bool abandoned_ = false;
  std::exception_ptr failure_;
  std::thread worker_;
};

// Chooses which of the `boundary_count` boundaries between consecutive
// stages get a thread handoff, for `threads` threads in total. Handoffs are
// spread evenly; with one thread there are none. The choice affects speed
// only, never the output.
inline std::vector<bool> PlaceHandoffs(std::size_t boundary_count,
                                       std::size_t threads) {
  std::vector<bool> placed(boundary_count, false);
  if (threads <= 1 || boundary_count == 0) {
    return placed;
  }
  const std::size_t handoffs = std::min(threads - 1, boundary_count);
  for (std::size_t index = 1; index <= handoffs; ++index) {
    // Boundary k sits after stage k; place handoff i at the i-th of
    // `handoffs + 1` equal divisions of the stage list.
    const std::size_t boundary =
        (index * (boundary_count + 1)) / (handoffs + 1) - 1;
    placed[boundary] = true;
  }
  return placed;
}

// Drives `source` to the end, passing each block to `consumer`, then closes
// the consumer.
inline void RunVideoSource(ICvbsInput* source,
                           IBlockConsumer<IreSampleBlock>* consumer) {
  assert(source != nullptr);
  assert(consumer != nullptr);
  IreSampleBlock block;
  while (source->ReadBlock(&block)) {
    consumer->Consume(std::move(block));
    block = IreSampleBlock{};
  }
  consumer->Close();
}

}  // namespace ld_creator
