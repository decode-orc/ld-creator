/*
 * File:        run_chain.h
 * Module:      pipeline
 * Purpose:     Shared plumbing of a run: taps, thread handoffs, hashing
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ld_creator/diagnostics/byte_sink.h"
#include "ld_creator/diagnostics/tap_writer.h"
#include "ld_creator/pipeline/block_chain.h"
#include "ld_creator/pipeline/run_setup.h"
#include "ld_creator/project/sha256.h"

namespace ld_creator {

struct RunOutputServices {
  IByteSourceFactory* byte_sources = nullptr;
  IByteSinkFactory* byte_sinks = nullptr;
};

// What a run wrote.
struct RfRunResult {
  std::int64_t rf_samples = 0;
  std::int64_t clipped_samples = 0;
  std::string rf_sha256;
  std::string resolved_path;
};

// Blocks queued at each thread handoff; affects memory and speed only.
inline constexpr std::size_t kHandoffCapacity = 4;

// Passes bytes on and hashes them.
class HashingByteSink final : public IByteSink {
 public:
  explicit HashingByteSink(std::unique_ptr<IByteSink> sink)
      : sink_(std::move(sink)) {}
  void Write(std::span<const std::byte> bytes) override {
    hash_.Update(bytes);
    sink_->Write(bytes);
  }
  void Close() override { sink_->Close(); }
  std::string HexDigest() { return hash_.HexDigest(); }

 private:
  std::unique_ptr<IByteSink> sink_;
  Sha256 hash_;
};

// The end of a chain: S14.
class RfSinkConsumer final : public IBlockConsumer<RfCodeBlock> {
 public:
  explicit RfSinkConsumer(IRfSampleSink* sink) : sink_(sink) {}
  void Consume(RfCodeBlock&& block) override { sink_->Write(block); }
  void Close() override { sink_->Finish(); }

 private:
  IRfSampleSink* sink_;
};

// The samples a sampled tap records from each block type.
inline std::span<const double> TapSamples(const IreSampleBlock& block) {
  return block.ire;
}
inline std::span<const double> TapSamples(const CarrierPhaseBlock& block) {
  return block.phase_rad;
}
inline std::span<const double> TapSamples(const PhotocurrentBlock& block) {
  return block.level;
}
inline std::span<const double> TapSamples(const ReadoutBlock& block) {
  return block.photocurrent.level;
}
inline std::int64_t TapFirstSample(const IreSampleBlock& block) {
  return block.first_sample;
}
inline std::int64_t TapFirstSample(const CarrierPhaseBlock& block) {
  return block.first_sample;
}
inline std::int64_t TapFirstSample(const PhotocurrentBlock& block) {
  return block.first_sample;
}
inline std::int64_t TapFirstSample(const ReadoutBlock& block) {
  return block.photocurrent.first_sample;
}

// Writes the passing stream to a sampled tap, then forwards it unchanged.
template <typename Block>
class SampledTapNode final : public IBlockConsumer<Block> {
 public:
  SampledTapNode(std::unique_ptr<SampledTapWriter> writer,
                 IBlockConsumer<Block>* downstream)
      : writer_(std::move(writer)), downstream_(downstream) {}
  void Consume(Block&& block) override {
    writer_->Write(TapFirstSample(block), TapSamples(block));
    downstream_->Consume(std::move(block));
  }
  void Close() override {
    writer_->Finish();
    downstream_->Close();
  }

 private:
  std::unique_ptr<SampledTapWriter> writer_;
  IBlockConsumer<Block>* downstream_;
};

// Writes the passing edges to an edge tap, then forwards them unchanged.
template <typename Block>
class EdgeTapNode final : public IBlockConsumer<Block> {
 public:
  EdgeTapNode(std::unique_ptr<EdgeTapWriter> writer,
              IBlockConsumer<Block>* downstream)
      : writer_(std::move(writer)), downstream_(downstream) {}
  void Consume(Block&& block) override {
    writer_->Write(std::span(block.edges));
    downstream_->Consume(std::move(block));
  }
  void Close() override {
    writer_->Finish();
    downstream_->Close();
  }

 private:
  std::unique_ptr<EdgeTapWriter> writer_;
  IBlockConsumer<Block>* downstream_;
};

// A sampled stream's tap description. The stream has
// samples_per_frame_numerator / samples_per_frame_denominator samples per
// frame; frame f starts at the first sample at or after f frame periods.
struct StreamTap {
  const char* stage;
  const char* representation;
  const char* unit;
  SampleClock clock;
  std::int64_t samples_per_frame_numerator;
  std::int64_t samples_per_frame_denominator = 1;

  // Index of the first sample of frame `frame`, ⌈f·num/den⌉.
  [[nodiscard]] std::int64_t FrameStart(std::int64_t frame) const {
    const std::int64_t product = frame * samples_per_frame_numerator;
    return (product + samples_per_frame_denominator - 1) /
           samples_per_frame_denominator;
  }
};

// Owns a chain's nodes and destroys them newest first, so every thread
// handoff stops before the nodes downstream of it go.
class ChainNodes {
 public:
  ChainNodes() = default;
  ChainNodes(const ChainNodes&) = delete;
  ChainNodes& operator=(const ChainNodes&) = delete;
  ~ChainNodes() {
    while (!nodes_.empty()) {
      nodes_.pop_back();
    }
  }
  template <typename Node, typename... Args>
  Node* Add(Args&&... args) {
    auto node = std::make_shared<Node>(std::forward<Args>(args)...);
    Node* raw = node.get();
    nodes_.push_back(std::move(node));
    return raw;
  }

 private:
  std::vector<std::shared_ptr<void>> nodes_;
};

// Builds a chain from its end backwards: taps and thread handoffs in front
// of each stage.
class ChainBuilder {
 public:
  ChainBuilder(const PreparedRun& run, IByteSinkFactory* sinks,
               unsigned int threads, std::size_t boundaries)
      : run_(run),
        sinks_(sinks),
        handoffs_(PlaceHandoffs(boundaries, threads)) {}

  // A sampled tap writer for one declared tap of `tap`'s stream.
  std::unique_ptr<SampledTapWriter> MakeSampledWriter(const TapConfig& config,
                                                      const StreamTap& tap) {
    return std::make_unique<SampledTapWriter>(
        sinks_, config.path,
        TapIdentity{.stage = tap.stage,
                    .representation = tap.representation,
                    .unit = tap.unit,
                    .first_frame = config.first_frame,
                    .frame_count = config.frame_count},
        tap.clock, tap.FrameStart(config.first_frame),
        tap.FrameStart(config.first_frame + config.frame_count));
  }

  // Puts the declared taps of `tap`'s stream in front of `downstream`, then
  // a thread handoff if boundary `boundary` has one.
  template <typename Block>
  IBlockConsumer<Block>* Upstream(IBlockConsumer<Block>* downstream,
                                  const StreamTap& tap, std::size_t boundary) {
    IBlockConsumer<Block>* consumer = downstream;
    for (const TapConfig& config : run_.config.taps) {
      if (config.stage == tap.stage) {
        consumer = nodes_.Add<SampledTapNode<Block>>(
            MakeSampledWriter(config, tap), consumer);
      }
    }
    return Handoff(consumer, boundary);
  }

  // Puts the declared edge taps of `stage` in front of `downstream`, then
  // a thread handoff if boundary `boundary` has one. `range` maps a tap's
  // frame range to the range of the edges' first field.
  template <typename Block, typename Range>
  IBlockConsumer<Block>* UpstreamEdges(
      IBlockConsumer<Block>* downstream, const char* stage,
      const char* representation, const char* unit, EdgeRecordLayout layout,
      const Range& range, std::size_t boundary) {
    IBlockConsumer<Block>* consumer = downstream;
    for (const TapConfig& config : run_.config.taps) {
      if (config.stage != stage) {
        continue;
      }
      const std::pair<double, double> span =
          range(config.first_frame, config.first_frame + config.frame_count);
      auto writer = std::make_unique<EdgeTapWriter>(
          sinks_, config.path,
          TapIdentity{.stage = stage,
                      .representation = representation,
                      .unit = unit,
                      .first_frame = config.first_frame,
                      .frame_count = config.frame_count},
          layout, span.first, span.second);
      consumer = nodes_.Add<EdgeTapNode<Block>>(std::move(writer), consumer);
    }
    return Handoff(consumer, boundary);
  }

  template <typename Block>
  IBlockConsumer<Block>* Handoff(IBlockConsumer<Block>* downstream,
                                 std::size_t boundary) {
    if (handoffs_[boundary]) {
      return nodes_.Add<ThreadedHandoff<Block>>(downstream, kHandoffCapacity);
    }
    return downstream;
  }

  ChainNodes& Nodes() { return nodes_; }

 private:
  const PreparedRun& run_;
  IByteSinkFactory* sinks_;
  std::vector<bool> handoffs_;
  ChainNodes nodes_;
};

// SHA-256 of the run's .cvbs and .meta, and this build's version.
ResolvedRecord HashRunInputs(const PreparedRun& run,
                             const std::string& ld_creator_version,
                             IByteSourceFactory* sources);

// Writes the resolved project beside the RF and returns its path.
std::string WriteResolvedProjectFile(const PreparedRun& run,
                                     const ResolvedRecord& record,
                                     IByteSinkFactory* sinks);

}  // namespace ld_creator
