/*
 * File:        disc_run.cpp
 * Module:      pipeline
 * Purpose:     Runs S1–S14: the RF of a modelled disc, with taps
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/pipeline/disc_run.h"

#include <cassert>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "ld_creator/cvbs_input/cvbs_video_input.h"
#include "ld_creator/disc_cutter/disc_cutter.h"
#include "ld_creator/disc_replicator/disc_replicator.h"
#include "ld_creator/fm_phase_modulator/fm_phase_modulator.h"
#include "ld_creator/group_delay_predistorter/group_delay_predistorter.h"
#include "ld_creator/optical_readout/optical_readout.h"
#include "ld_creator/optical_readout/readout_kernel.h"
#include "ld_creator/playback_mechanics/playback_mechanics.h"
#include "ld_creator/player_front_end/player_front_end.h"
#include "ld_creator/recording_limiter/recording_limiter.h"
#include "ld_creator/recording_signal_combiner/recording_signal_combiner.h"
#include "ld_creator/rf_capture/capture_quantiser.h"
#include "ld_creator/rf_capture/rf_capture.h"
#include "ld_creator/rf_sample_writer/rf_sample_writer.h"
#include "ld_creator/standards/ntsc_timing.h"
#include "ld_creator/video_pre_emphasis/video_pre_emphasis.h"
#include "ld_creator/video_rate_converter/video_rate_converter.h"

namespace ld_creator {

namespace {

// Lines per S1 block, as in the carrier diagnostic. Affects memory and
// speed only.
constexpr std::int64_t kLinesPerBlock = 35;

// Fm = 24·fsc and Fr = 80 MHz: Fr/Fm = 176/189 and 40 MHz/Fm = 88/189
// exactly (fsc = 315/88 MHz).
constexpr std::int64_t kModellingRatioDenominator = 189;
constexpr std::int64_t kReadoutRatioNumerator = 176;
constexpr std::int64_t kCaptureRatioNumerator = 88;

// Readout samples per frame: 80 MHz × 1001/30000 s = 8 008 000/3.
constexpr std::int64_t kReadoutSamplesPerThreeFrames = 8'008'000;

// One frame period, 1001/30000 s.
double FrameTime(std::int64_t frame) {
  return static_cast<double>(frame) / ntsc::kFrameRateHz;
}

std::int64_t CeilRatio(std::int64_t samples, std::int64_t numerator) {
  return ((samples * numerator) + kModellingRatioDenominator - 1) /
         kModellingRatioDenominator;
}

// What one pass of the chain leaves behind.
struct ChainResult {
  std::int64_t clipped = 0;
  double lowest = 0.0;
  double highest = 0.0;
};

// The measuring pass's S14: discards the codes.
class DiscardingRfSink final : public IRfSampleSink {
 public:
  void Write(const RfCodeBlock& /*block*/) override {}
  void Finish() override {}
};

// Runs S1 → … → S13 into `sink` once, writing `run`'s taps. The capture
// level comes from `level_for_contrast`, given the readout's pit contrast.
ChainResult RunDiscChain(
    const PreparedRun& run, unsigned int threads,
    const RunOutputServices& services,
    const std::function<CaptureLevel(double)>& level_for_contrast,
    IRfSampleSink* sink) {
  const ProjectConfig& config = run.config;
  const std::unique_ptr<IByteSource> video =
      services.byte_sources->Open(config.input.video_path);

  // S1–S6.
  CvbsVideoInput input(video.get(), run.encoding, config.input.first_frame,
                       run.frame_count, kLinesPerBlock);
  VideoRateConverter rate_converter;
  GroupDelayPredistorter predistorter(config.group_delay_predistorter);
  VideoPreEmphasis pre_emphasis(config.video_pre_emphasis);
  FmPhaseModulator modulator(config.fm_phase_modulator);
  PassThroughSignalCombiner combiner;
  // S7–S10.
  RecordingLimiter limiter;
  DiscCutter cutter(config.disc_cutter);
  const CuttingTrajectory& cutting = cutter.Trajectory();
  ExactCopyReplicator replicator;
  LockedPlaybackMechanics mechanics(cutting);
  const ISpotTrajectory& spot = mechanics.Trajectory();
  // S11–S13.
  const ReadoutExtent extent{
      .sample_count = ReadoutSampleCount(run.frame_count),
      .end_arc_length_m = cutting.ArcLengthAt(FrameTime(run.frame_count))};
  LinearOpticalReadout readout(config.optical_readout, &spot, extent);
  std::unique_ptr<IPlayerFrontEnd> front_end;
  if (config.player_front_end.mtf_equaliser == MtfEqualiser::kInverse) {
    front_end = std::make_unique<InverseMtfEqualiser>(
        config.optical_readout.numerical_aperture, readout.Contrast(), &spot,
        extent);
  } else {
    front_end = std::make_unique<ReferenceFrontEnd>();
  }

  ChainBuilder chain(run, services.byte_sinks, threads, 13);
  const SampleClock d1_clock{.rate_hz = ntsc::kSampleRateHz,
                             .origin_samples = ntsc::kStoredOriginSamples};
  const SampleClock fm_clock{.rate_hz = kModellingRateHz,
                             .origin_samples = 0.0};
  const SampleClock readout_clock{.rate_hz = kReadoutRateHz,
                                  .origin_samples = 0.0};
  const StreamTap capture_tap{"rf_capture",
                              "D9 filtered",
                              "ADC half-scale",
                              readout_clock,
                              kReadoutSamplesPerThreeFrames,
                              3};
  std::vector<std::unique_ptr<SampledTapWriter>> analogue_taps;
  for (const TapConfig& tap : config.taps) {
    if (tap.stage == capture_tap.stage) {
      analogue_taps.push_back(chain.MakeSampledWriter(tap, capture_tap));
    }
  }
  const CaptureChain capture_chain =
      MakeCaptureChain(config.rf_capture.profile);
  RfCapture capture(config.rf_capture.profile, capture_chain,
                    level_for_contrast(readout.Contrast()),
                    std::move(analogue_taps));

  const auto time_range = [](std::int64_t first, std::int64_t end) {
    return std::pair<double, double>(FrameTime(first), FrameTime(end));
  };
  const auto arc_range = [&cutting](std::int64_t first, std::int64_t end) {
    return std::pair<double, double>(cutting.ArcLengthAt(FrameTime(first)),
                                     cutting.ArcLengthAt(FrameTime(end)));
  };

  // Source → S2 → … → S13 → S14: thirteen boundaries, numbered from the
  // source.
  ChainNodes& nodes = chain.Nodes();
  {
    IBlockConsumer<RfCodeBlock>* to_sink =
        chain.Handoff(static_cast<IBlockConsumer<RfCodeBlock>*>(
                          nodes.Add<RfSinkConsumer>(sink)),
                      12);
    auto* s13 =
        nodes.Add<StageNode<PhotocurrentBlock, RfCodeBlock>>(&capture, to_sink);
    auto* to_s13 =
        chain.Upstream(static_cast<IBlockConsumer<PhotocurrentBlock>*>(s13),
                       {"player_front_end", "D9", "land = 1", readout_clock,
                        kReadoutSamplesPerThreeFrames, 3},
                       11);
    auto* s12 = nodes.Add<StageNode<ReadoutBlock, PhotocurrentBlock>>(
        front_end.get(), to_s13);
    auto* to_s12 =
        chain.Upstream(static_cast<IBlockConsumer<ReadoutBlock>*>(s12),
                       {"optical_readout", "D8", "land = 1", readout_clock,
                        kReadoutSamplesPerThreeFrames, 3},
                       10);
    auto* s11 = nodes.Add<StageNode<ScanBlock, ReadoutBlock>>(&readout, to_s12);
    auto* to_s11 =
        chain.Handoff(static_cast<IBlockConsumer<ScanBlock>*>(s11), 9);
    auto* s10 =
        nodes.Add<StageNode<PitEdgeBlock, ScanBlock>>(&mechanics, to_s11);
    auto* to_s10 =
        chain.Handoff(static_cast<IBlockConsumer<PitEdgeBlock>*>(s10), 8);
    auto* s9 =
        nodes.Add<StageNode<PitEdgeBlock, PitEdgeBlock>>(&replicator, to_s10);
    auto* to_s9 = chain.UpstreamEdges(
        static_cast<IBlockConsumer<PitEdgeBlock>*>(s9), "disc_cutter", "D5",
        "m", EdgeRecordLayout::kPitEdge, arc_range, 7);
    auto* s8 =
        nodes.Add<StageNode<RecordedEdgeBlock, PitEdgeBlock>>(&cutter, to_s9);
    auto* to_s8 =
        chain.UpstreamEdges(static_cast<IBlockConsumer<RecordedEdgeBlock>*>(s8),
                            "recording_limiter", "D4", "s",
                            EdgeRecordLayout::kRecordedEdge, time_range, 6);
    auto* s7 = nodes.Add<StageNode<CarrierPhaseBlock, RecordedEdgeBlock>>(
        &limiter, to_s8);
    auto* to_s7 =
        chain.Handoff(static_cast<IBlockConsumer<CarrierPhaseBlock>*>(s7), 5);
    auto* s6 = nodes.Add<StageNode<CarrierPhaseBlock, CarrierPhaseBlock>>(
        &combiner, to_s7);
    auto* to_s6 =
        chain.Upstream(static_cast<IBlockConsumer<CarrierPhaseBlock>*>(s6),
                       {"fm_phase_modulator", "D3", "rad", fm_clock,
                        kModellingSamplesPerFrame},
                       4);
    auto* s5 = nodes.Add<StageNode<IreSampleBlock, CarrierPhaseBlock>>(
        &modulator, to_s6);
    auto* to_s5 =
        chain.Upstream(static_cast<IBlockConsumer<IreSampleBlock>*>(s5),
                       {"video_pre_emphasis", "D2", "IRE", fm_clock,
                        kModellingSamplesPerFrame},
                       3);
    auto* s4 = nodes.Add<StageNode<IreSampleBlock, IreSampleBlock>>(
        &pre_emphasis, to_s5);
    auto* to_s4 =
        chain.Upstream(static_cast<IBlockConsumer<IreSampleBlock>*>(s4),
                       {"group_delay_predistorter", "D2", "IRE", fm_clock,
                        kModellingSamplesPerFrame},
                       2);
    auto* s3 = nodes.Add<StageNode<IreSampleBlock, IreSampleBlock>>(
        &predistorter, to_s4);
    auto* to_s3 =
        chain.Upstream(static_cast<IBlockConsumer<IreSampleBlock>*>(s3),
                       {"video_rate_converter", "D2", "IRE", fm_clock,
                        kModellingSamplesPerFrame},
                       1);
    auto* s2 = nodes.Add<StageNode<IreSampleBlock, IreSampleBlock>>(
        &rate_converter, to_s3);
    auto* to_s2 = chain.Upstream(
        static_cast<IBlockConsumer<IreSampleBlock>*>(s2),
        {"cvbs_input", "D1", "IRE", d1_clock, ntsc::kSamplesPerFrame}, 0);
    // Closing the chain finishes every stage and closes the RF file.
    RunVideoSource(&input, to_s2);
  }
  return {.clipped = capture.ClippedSamples(),
          .lowest = capture.LowestSampled(),
          .highest = capture.HighestSampled()};
}

}  // namespace

std::int64_t ReadoutSampleCount(std::int64_t frame_count) {
  return CeilRatio(frame_count * kModellingSamplesPerFrame,
                   kReadoutRatioNumerator);
}

std::int64_t CaptureSampleCount(std::int64_t frame_count) {
  return CeilRatio(frame_count * kModellingSamplesPerFrame,
                   kCaptureRatioNumerator);
}

RfRunResult RunDisc(const PreparedRun& run,
                    const std::string& ld_creator_version, unsigned int threads,
                    const RunOutputServices& services) {
  assert(services.byte_sources != nullptr);
  assert(services.byte_sinks != nullptr);
  const ProjectConfig& config = run.config;

  ResolvedRecord record =
      HashRunInputs(run, ld_creator_version, services.byte_sources);

  // The level rule (S13 note, "Level"): a first pass, without taps or
  // output, measures the run's filtered RF at the converter's samples; the
  // second scales its peak-to-peak to level_fraction of the window.
  PreparedRun measuring = run;
  measuring.config.taps.clear();
  DiscardingRfSink discard;
  const ChainResult measured = RunDiscChain(
      measuring, threads, services,
      [](double contrast) {
        return MeasuringCaptureLevel(CaptureMean(contrast));
      },
      &discard);

  auto rf_bytes = std::make_unique<HashingByteSink>(
      services.byte_sinks->Create(config.output.rf_path));
  HashingByteSink* rf_hash = rf_bytes.get();
  RfSampleWriter writer(config.rf_capture.profile, rf_bytes.get());
  CaptureLevel level;
  const ChainResult written = RunDiscChain(
      run, threads, services,
      [&](double contrast) {
        level = LevelFromExtremes(config.rf_capture, CaptureMean(contrast),
                                  measured.lowest, measured.highest);
        return level;
      },
      &writer);

  RfRunResult result;
  result.rf_samples = writer.SamplesWritten();
  result.clipped_samples = written.clipped;
  record.capture_peak_to_peak = level.peak_to_peak;
  record.capture_scale = level.scale;
  result.rf_sha256 = rf_hash->HexDigest();
  record.rf_sha256 = result.rf_sha256;
  result.resolved_path =
      WriteResolvedProjectFile(run, record, services.byte_sinks);
  return result;
}

}  // namespace ld_creator
