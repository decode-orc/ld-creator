/*
 * File:        carrier_diagnostic_run.cpp
 * Module:      pipeline
 * Purpose:     Runs S1–S5 and the carrier diagnostic, writing RF and taps
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/pipeline/carrier_diagnostic_run.h"

#include <cassert>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "ld_creator/carrier_diagnostic/carrier_diagnostic_renderer.h"
#include "ld_creator/diagnostics/tap_writer.h"
#include "ld_creator/fm_phase_modulator/fm_phase_modulator.h"
#include "ld_creator/group_delay_predistorter/group_delay_predistorter.h"
#include "ld_creator/pipeline/block_chain.h"
#include "ld_creator/project/resolved_project.h"
#include "ld_creator/project/sha256.h"
#include "ld_creator/rf_sample_writer/rf_sample_writer.h"
#include "ld_creator/standards/ntsc_timing.h"
#include "ld_creator/video_pre_emphasis/video_pre_emphasis.h"
#include "ld_creator/video_rate_converter/video_rate_converter.h"

namespace ld_creator {

namespace {

// Lines per S1 block: 35 lines (31 850 samples), 15 blocks per frame. Affects
// memory and speed only.
constexpr std::int64_t kLinesPerBlock = 35;

// Blocks queued at each thread handoff.
constexpr std::size_t kHandoffCapacity = 4;

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

// The end of the chain: S14.
class RfSinkConsumer final : public IBlockConsumer<RfCodeBlock> {
 public:
  explicit RfSinkConsumer(IRfSampleSink* sink) : sink_(sink) {}
  void Consume(RfCodeBlock&& block) override { sink_->Write(block); }
  void Close() override { sink_->Finish(); }

 private:
  IRfSampleSink* sink_;
};

std::span<const double> TapSamples(const IreSampleBlock& block) {
  return block.ire;
}
std::span<const double> TapSamples(const CarrierPhaseBlock& block) {
  return block.phase_rad;
}

// Writes the passing stream to a tap, then forwards it unchanged.
template <typename Block>
class SampledTapNode final : public IBlockConsumer<Block> {
 public:
  SampledTapNode(std::unique_ptr<SampledTapWriter> writer,
                 IBlockConsumer<Block>* downstream)
      : writer_(std::move(writer)), downstream_(downstream) {}
  void Consume(Block&& block) override {
    writer_->Write(block.first_sample, TapSamples(block));
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

// One stream's tap description.
struct StreamTap {
  const char* stage;
  const char* representation;
  const char* unit;
  SampleClock clock;
  std::int64_t samples_per_frame;
};

// Owns the chain's nodes and destroys them newest first, so every thread
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

class ChainBuilder {
 public:
  ChainBuilder(const PreparedRun& run, IByteSinkFactory* sinks,
               unsigned int threads, std::size_t boundaries)
      : run_(run),
        sinks_(sinks),
        handoffs_(PlaceHandoffs(boundaries, threads)) {}

  // Puts the taps for `tap` in front of `downstream`, then a thread handoff
  // if boundary `boundary` has one.
  template <typename Block>
  IBlockConsumer<Block>* Upstream(IBlockConsumer<Block>* downstream,
                                  const StreamTap& tap, std::size_t boundary) {
    IBlockConsumer<Block>* consumer = downstream;
    for (const TapConfig& config : run_.config.taps) {
      if (config.stage != tap.stage) {
        continue;
      }
      const std::int64_t begin = config.first_frame * tap.samples_per_frame;
      const std::int64_t end =
          begin + (config.frame_count * tap.samples_per_frame);
      auto writer = std::make_unique<SampledTapWriter>(
          sinks_, config.path,
          TapIdentity{.stage = tap.stage,
                      .representation = tap.representation,
                      .unit = tap.unit,
                      .first_frame = config.first_frame,
                      .frame_count = config.frame_count},
          tap.clock, begin, end);
      consumer = nodes_.Add<SampledTapNode<Block>>(std::move(writer), consumer);
    }
    if (handoffs_[boundary]) {
      consumer = nodes_.Add<ThreadedHandoff<Block>>(consumer, kHandoffCapacity);
    }
    return consumer;
  }

  [[nodiscard]] bool HasHandoff(std::size_t boundary) const {
    return handoffs_[boundary];
  }
  ChainNodes& Nodes() { return nodes_; }

 private:
  const PreparedRun& run_;
  IByteSinkFactory* sinks_;
  std::vector<bool> handoffs_;
  ChainNodes nodes_;
};

}  // namespace

CarrierDiagnosticResult RunCarrierDiagnostic(
    const PreparedRun& run, const std::string& ld_creator_version,
    unsigned int threads, const RunOutputServices& services) {
  assert(services.byte_sources != nullptr);
  assert(services.byte_sinks != nullptr);
  const ProjectConfig& config = run.config;

  ResolvedRecord record;
  record.ld_creator_version = ld_creator_version;
  {
    const std::unique_ptr<IByteSource> meta =
        services.byte_sources->Open(run.meta_path);
    record.meta_sha256 = HashByteSource(meta.get());
  }
  const std::unique_ptr<IByteSource> video =
      services.byte_sources->Open(config.input.video_path);
  record.cvbs_sha256 = HashByteSource(video.get());

  CvbsVideoInput input(video.get(), run.encoding, config.input.first_frame,
                       run.frame_count, kLinesPerBlock);
  VideoRateConverter rate_converter;
  GroupDelayPredistorter predistorter(config.group_delay_predistorter);
  VideoPreEmphasis pre_emphasis(config.video_pre_emphasis);
  FmPhaseModulator modulator(config.fm_phase_modulator);
  CarrierDiagnosticRenderer renderer(config.rf_capture);

  auto rf_bytes = std::make_unique<HashingByteSink>(
      services.byte_sinks->Create(config.output.rf_path));
  HashingByteSink* rf_hash = rf_bytes.get();
  RfSampleWriter writer(config.rf_capture.profile, rf_bytes.get());

  const SampleClock d1_clock{.rate_hz = ntsc::kSampleRateHz,
                             .origin_samples = ntsc::kStoredOriginSamples};
  const SampleClock fm_clock{.rate_hz = kModellingRateHz,
                             .origin_samples = 0.0};

  // Source → S2 → S3 → S4 → S5 → renderer → S14: six boundaries.
  ChainBuilder chain(run, services.byte_sinks, threads, 6);
  ChainNodes& nodes = chain.Nodes();
  {
    auto* sink = nodes.Add<RfSinkConsumer>(&writer);
    IBlockConsumer<RfCodeBlock>* to_sink = sink;
    // No taps exist between the renderer and S14 in this mode.
    if (chain.HasHandoff(5)) {
      to_sink =
          nodes.Add<ThreadedHandoff<RfCodeBlock>>(to_sink, kHandoffCapacity);
    }
    auto* render_node = nodes.Add<StageNode<CarrierPhaseBlock, RfCodeBlock>>(
        &renderer, to_sink);
    IBlockConsumer<CarrierPhaseBlock>* to_render = chain.Upstream(
        static_cast<IBlockConsumer<CarrierPhaseBlock>*>(render_node),
        {"fm_phase_modulator", "D3", "rad", fm_clock,
         kModellingSamplesPerFrame},
        4);
    auto* s5_node = nodes.Add<StageNode<IreSampleBlock, CarrierPhaseBlock>>(
        &modulator, to_render);
    IBlockConsumer<IreSampleBlock>* to_s5 =
        chain.Upstream(static_cast<IBlockConsumer<IreSampleBlock>*>(s5_node),
                       {"video_pre_emphasis", "D2", "IRE", fm_clock,
                        kModellingSamplesPerFrame},
                       3);
    auto* s4_node = nodes.Add<StageNode<IreSampleBlock, IreSampleBlock>>(
        &pre_emphasis, to_s5);
    IBlockConsumer<IreSampleBlock>* to_s4 =
        chain.Upstream(static_cast<IBlockConsumer<IreSampleBlock>*>(s4_node),
                       {"group_delay_predistorter", "D2", "IRE", fm_clock,
                        kModellingSamplesPerFrame},
                       2);
    auto* s3_node = nodes.Add<StageNode<IreSampleBlock, IreSampleBlock>>(
        &predistorter, to_s4);
    IBlockConsumer<IreSampleBlock>* to_s3 =
        chain.Upstream(static_cast<IBlockConsumer<IreSampleBlock>*>(s3_node),
                       {"video_rate_converter", "D2", "IRE", fm_clock,
                        kModellingSamplesPerFrame},
                       1);
    auto* s2_node = nodes.Add<StageNode<IreSampleBlock, IreSampleBlock>>(
        &rate_converter, to_s3);
    IBlockConsumer<IreSampleBlock>* to_s2 = chain.Upstream(
        static_cast<IBlockConsumer<IreSampleBlock>*>(s2_node),
        {"cvbs_input", "D1", "IRE", d1_clock, ntsc::kSamplesPerFrame}, 0);
    // Closing the chain finishes every stage and closes the RF file.
    RunVideoSource(&input, to_s2);
  }

  CarrierDiagnosticResult result;
  result.rf_samples = writer.SamplesWritten();
  result.clipped_samples = renderer.ClippedSamples();
  result.rf_sha256 = rf_hash->HexDigest();
  record.rf_sha256 = result.rf_sha256;

  result.resolved_path = ResolvedProjectPath(config.output.rf_path);
  const std::unique_ptr<IByteSink> resolved =
      services.byte_sinks->Create(result.resolved_path);
  WriteText(resolved.get(),
            EmitResolvedProject(config, run.frame_count, record));
  resolved->Close();
  return result;
}

}  // namespace ld_creator
