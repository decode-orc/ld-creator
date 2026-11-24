/*
 * File:        test_block_chain.cpp
 * Module:      pipeline
 * Purpose:     Unit tests: chained stages are independent of blocks and threads
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "ld_creator/pipeline/block_chain.h"

namespace ld_creator {
namespace {

// A source that emits a fixed ramp-like signal in blocks of `block_size`.
class FakeVideoSource final : public ICvbsInput {
 public:
  FakeVideoSource(std::int64_t total, std::int64_t block_size)
      : total_(total), block_size_(block_size) {}

  [[nodiscard]] VideoInputStream Stream() const override {
    return {.clock = {.rate_hz = 1.0, .origin_samples = 0.0}, .frame_count = 1};
  }

  bool ReadBlock(IreSampleBlock* block) override {
    if (next_ >= total_) {
      return false;
    }
    const std::int64_t count = std::min(block_size_, total_ - next_);
    block->first_sample = next_;
    block->ire.clear();
    for (std::int64_t index = next_; index < next_ + count; ++index) {
      // A deterministic signal that changes sign irregularly.
      block->ire.push_back(static_cast<double>((index * 37) % 101) - 50.0);
    }
    next_ += count;
    return true;
  }

 private:
  std::int64_t total_;
  std::int64_t block_size_;
  std::int64_t next_ = 0;
};

// Stateful: a running sum, which is wrong if history is lost between blocks.
class RunningSumStage final : public IVideoPreEmphasis {
 public:
  void Process(const IreSampleBlock& input, IreSampleBlock* output) override {
    output->first_sample = input.first_sample;
    output->ire.clear();
    for (const double value : input.ire) {
      sum_ += value;
      output->ire.push_back(sum_);
    }
  }
  void Finish(IreSampleBlock* output) override { output->ire.clear(); }

 private:
  double sum_ = 0.0;
};

// Re-chunks: averages pairs of samples, holding an odd sample over to the
// next block, and emits a held sample on Finish().
class PairAverageStage final
    : public IBlockStage<IreSampleBlock, CarrierPhaseBlock> {
 public:
  void Process(const IreSampleBlock& input,
               CarrierPhaseBlock* output) override {
    output->first_sample = next_output_;
    output->phase_rad.clear();
    output->slice_level.clear();
    for (const double value : input.ire) {
      if (held_) {
        Emit((*held_ + value) / 2.0, output);
        held_.reset();
      } else {
        held_ = value;
      }
    }
  }
  void Finish(CarrierPhaseBlock* output) override {
    output->first_sample = next_output_;
    output->phase_rad.clear();
    output->slice_level.clear();
    if (held_) {
      Emit(*held_, output);
      held_.reset();
    }
  }

 private:
  void Emit(double value, CarrierPhaseBlock* output) {
    output->phase_rad.push_back(value);
    output->slice_level.push_back(0.0);
    ++next_output_;
  }
  std::optional<double> held_;
  std::int64_t next_output_ = 0;
};

// Emits an edge at each sign change of the phase, as the limiter will.
class SignChangeStage final : public IRecordingLimiter {
 public:
  void Process(const CarrierPhaseBlock& input,
               RecordedEdgeBlock* output) override {
    output->edges.clear();
    for (std::size_t i = 0; i < input.phase_rad.size(); ++i) {
      const bool positive = input.phase_rad[i] > 0.0;
      if (previous_ && *previous_ != positive) {
        output->edges.push_back(
            {.time_s = static_cast<double>(input.first_sample +
                                           static_cast<std::int64_t>(i)),
             .polarity =
                 positive ? EdgePolarity::kPitStart : EdgePolarity::kPitEnd});
      }
      previous_ = positive;
    }
    output->complete_until_s = static_cast<double>(
        input.first_sample + static_cast<std::int64_t>(input.phase_rad.size()));
  }
  void Finish(RecordedEdgeBlock* output) override { output->edges.clear(); }

 private:
  std::optional<bool> previous_;
};

class CollectingSink final : public IBlockConsumer<RecordedEdgeBlock> {
 public:
  void Consume(RecordedEdgeBlock&& block) override {
    for (const RecordedEdge& edge : block.edges) {
      times.push_back(edge.time_s);
      polarities.push_back(edge.polarity);
    }
    watermarks.push_back(block.complete_until_s);
  }
  void Close() override { closed = true; }

  std::vector<double> times;
  std::vector<EdgePolarity> polarities;
  std::vector<double> watermarks;
  bool closed = false;
};

struct ChainOutput {
  std::vector<double> times;
  std::vector<EdgePolarity> polarities;
};

// Runs source → running sum → pair average → sign change → sink, with
// handoffs placed for `threads` threads.
ChainOutput RunChain(std::int64_t block_size, std::size_t threads) {
  constexpr std::int64_t kSamples = 10007;
  constexpr std::size_t kQueueCapacity = 4;
  FakeVideoSource source(kSamples, block_size);
  RunningSumStage running_sum;
  PairAverageStage pair_average;
  SignChangeStage sign_change;
  CollectingSink sink;

  const std::vector<bool> handoffs = PlaceHandoffs(3, threads);

  // Built back to front: each node needs its downstream consumer.
  StageNode<CarrierPhaseBlock, RecordedEdgeBlock> sign_node(&sign_change,
                                                            &sink);
  std::optional<ThreadedHandoff<CarrierPhaseBlock>> sign_handoff;
  IBlockConsumer<CarrierPhaseBlock>* to_sign = &sign_node;
  if (handoffs[2]) {
    sign_handoff.emplace(&sign_node, kQueueCapacity);
    to_sign = &*sign_handoff;
  }
  StageNode<IreSampleBlock, CarrierPhaseBlock> average_node(&pair_average,
                                                            to_sign);
  std::optional<ThreadedHandoff<IreSampleBlock>> average_handoff;
  IBlockConsumer<IreSampleBlock>* to_average = &average_node;
  if (handoffs[1]) {
    average_handoff.emplace(&average_node, kQueueCapacity);
    to_average = &*average_handoff;
  }
  StageNode<IreSampleBlock, IreSampleBlock> sum_node(&running_sum, to_average);
  std::optional<ThreadedHandoff<IreSampleBlock>> sum_handoff;
  IBlockConsumer<IreSampleBlock>* to_sum = &sum_node;
  if (handoffs[0]) {
    sum_handoff.emplace(&sum_node, kQueueCapacity);
    to_sum = &*sum_handoff;
  }

  RunVideoSource(&source, to_sum);
  EXPECT_TRUE(sink.closed);
  return {.times = sink.times, .polarities = sink.polarities};
}

TEST(BlockChainTest, OutputIsIdenticalAcrossBlockSizesAndThreadCounts) {
  const ChainOutput reference = RunChain(10007, 1);
  ASSERT_GT(reference.times.size(), 10U);
  for (const std::int64_t block_size : {1, 2, 3, 7, 910, 4096}) {
    for (const std::size_t threads : {1U, 2U, 3U, 4U, 8U}) {
      const ChainOutput output = RunChain(block_size, threads);
      EXPECT_EQ(output.times, reference.times)
          << "block size " << block_size << ", threads " << threads;
      EXPECT_EQ(output.polarities, reference.polarities)
          << "block size " << block_size << ", threads " << threads;
    }
  }
}

TEST(BlockChainTest, EmitsHeldOutputWhenStageFinishes) {
  FakeVideoSource source(3, 1);
  PairAverageStage stage;
  class Collect final : public IBlockConsumer<CarrierPhaseBlock> {
   public:
    void Consume(CarrierPhaseBlock&& block) override {
      count += block.phase_rad.size();
    }
    void Close() override {}
    std::size_t count = 0;
  } collect;
  StageNode<IreSampleBlock, CarrierPhaseBlock> node(&stage, &collect);

  RunVideoSource(&source, &node);

  // Three samples: one averaged pair plus the held third sample.
  EXPECT_EQ(collect.count, 2U);
}

TEST(BlockChainTest, RethrowsDownstreamFailureFromHandoff) {
  class Failing final : public IBlockConsumer<IreSampleBlock> {
   public:
    void Consume(IreSampleBlock&& /*block*/) override {
      throw std::runtime_error("stage failed");
    }
    void Close() override {}
  } failing;
  FakeVideoSource source(100, 10);
  ThreadedHandoff<IreSampleBlock> handoff(&failing, 2);

  EXPECT_THROW(RunVideoSource(&source, &handoff), std::runtime_error);
}

TEST(BlockChainTest, PlacesNoHandoffsForOneThread) {
  EXPECT_EQ(PlaceHandoffs(5, 1), std::vector<bool>(5, false));
  EXPECT_EQ(PlaceHandoffs(5, 0), std::vector<bool>(5, false));
}

TEST(BlockChainTest, PlacesOneHandoffPerExtraThreadUpToBoundaryCount) {
  for (std::size_t threads = 2; threads <= 10; ++threads) {
    const std::vector<bool> placed = PlaceHandoffs(5, threads);
    const auto count = static_cast<std::size_t>(
        std::count(placed.begin(), placed.end(), true));
    EXPECT_EQ(count, std::min<std::size_t>(threads - 1, 5)) << threads;
  }
}

}  // namespace
}  // namespace ld_creator
