/*
 * File:        stage_interfaces.h
 * Module:      pipeline
 * Purpose:     One streaming interface per pipeline stage (S1–S14)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>

#include "ld_creator/pipeline/pit_edges.h"
#include "ld_creator/pipeline/sampled_signal.h"

namespace ld_creator {

// A streaming stage that turns blocks of `In` into blocks of `Out`.
//
// Contract (block-size independence): the concatenation of everything a
// stage emits from Process() and Finish() depends only on the concatenation
// of its input, never on how that input was split into blocks. Stages keep
// whatever history they need between calls. This is what makes the RF
// independent of block size and thread count.
//
// Thread safety: a stage instance is used by one thread at a time; the
// pipeline never calls one instance concurrently.
template <typename In, typename Out>
class IBlockStage {
 public:
  virtual ~IBlockStage() = default;

  // Consumes `input` and replaces `*output` with the output that is now
  // final. `*output` may be left empty.
  virtual void Process(const In& input, Out* output) = 0;

  // Called once after the last input block: replaces `*output` with the
  // remaining output. No Process() call follows.
  virtual void Finish(Out* output) = 0;
};

// Stream properties of the S1 output, known once the input is opened.
struct VideoInputStream {
  SampleClock clock;
  std::int64_t frame_count = 0;
};

// S1 cvbs_input: produces D1. Each block holds whole lines and never crosses
// a frame boundary, so every frame starts on a block boundary (an NTSC field
// boundary falls mid-line 263, so line-aligned blocks cannot end exactly on
// one; frames are the finest unit both fields align to).
class ICvbsInput {
 public:
  virtual ~ICvbsInput() = default;

  [[nodiscard]] virtual VideoInputStream Stream() const = 0;

  // Fills `*block` with the next block and returns true, or returns false
  // at the end of the selected frames. Throws on a read or format error.
  virtual bool ReadBlock(IreSampleBlock* block) = 0;
};

// S2 video_rate_converter: D1 → D2.
class IVideoRateConverter : public IBlockStage<IreSampleBlock, IreSampleBlock> {
};

// S3 group_delay_predistorter: D2 → D2.
class IGroupDelayPredistorter
    : public IBlockStage<IreSampleBlock, IreSampleBlock> {};

// S4 video_pre_emphasis: D2 → D2.
class IVideoPreEmphasis : public IBlockStage<IreSampleBlock, IreSampleBlock> {};

// S5 fm_phase_modulator: D2 → D3.
class IFmPhaseModulator
    : public IBlockStage<IreSampleBlock, CarrierPhaseBlock> {};

// S6 recording_signal_combiner: D3 → D3.
class IRecordingSignalCombiner
    : public IBlockStage<CarrierPhaseBlock, CarrierPhaseBlock> {};

// S7 recording_limiter: D3 → D4.
class IRecordingLimiter
    : public IBlockStage<CarrierPhaseBlock, RecordedEdgeBlock> {};

// S8 disc_cutter: D4 → D5.
class IDiscCutter : public IBlockStage<RecordedEdgeBlock, PitEdgeBlock> {};

// S9 disc_replicator: D5 → D6.
class IDiscReplicator : public IBlockStage<PitEdgeBlock, PitEdgeBlock> {};

// S10 playback_mechanics: D6 → D7.
class IPlaybackMechanics : public IBlockStage<PitEdgeBlock, ScanBlock> {};

// S11 optical_readout: D7 → D8.
class IOpticalReadout : public IBlockStage<ScanBlock, PhotocurrentBlock> {};

// S12 player_front_end: D8 → D9. The diagnostic inverse equaliser also needs
// the D7 edges, which it receives from S10 when the pipeline is built.
class IPlayerFrontEnd
    : public IBlockStage<PhotocurrentBlock, PhotocurrentBlock> {};

// S13 rf_capture: D9 → D10.
class IRfCapture : public IBlockStage<PhotocurrentBlock, RfCodeBlock> {};

// S14 rf_sample_writer: consumes D10.
class IRfSampleSink {
 public:
  virtual ~IRfSampleSink() = default;

  // Writes `block`, which follows the previous block without a gap. Throws on
  // a code outside the profile's range or a write error.
  virtual void Write(const RfCodeBlock& block) = 0;

  // Flushes after the last block.
  virtual void Finish() = 0;
};

}  // namespace ld_creator
