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

}  // namespace

RfRunResult RunCarrierDiagnostic(const PreparedRun& run,
                                 const std::string& ld_creator_version,
                                 unsigned int threads,
                                 const RunOutputServices& services) {
  assert(services.byte_sources != nullptr);
  assert(services.byte_sinks != nullptr);
  const ProjectConfig& config = run.config;

  ResolvedRecord record =
      HashRunInputs(run, ld_creator_version, services.byte_sources);
  const std::unique_ptr<IByteSource> video =
      services.byte_sources->Open(config.input.video_path);

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
    to_sink = chain.Handoff(to_sink, 5);
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

  RfRunResult result;
  result.rf_samples = writer.SamplesWritten();
  result.clipped_samples = renderer.ClippedSamples();
  result.rf_sha256 = rf_hash->HexDigest();
  record.rf_sha256 = result.rf_sha256;
  result.resolved_path =
      WriteResolvedProjectFile(run, record, services.byte_sinks);
  return result;
}

}  // namespace ld_creator
