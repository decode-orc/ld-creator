/*
 * File:        video_rate_converter.h
 * Module:      video_rate_converter
 * Purpose:     S2: band-limited interpolation of 4fsc video to the modelling
 * rate
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "ld_creator/pipeline/stage_interfaces.h"
#include "ld_creator/standards/ntsc_timing.h"

namespace ld_creator {

// Modelling rate Fm = 6 × 4fsc = 24 fsc = 945/11 MHz ≈ 85.909 MHz (register
// S2.modelling_rate). The reason for the value is in
// docs-tech/user/disc-model/s02-video-rate-converter.md: cubic Hermite
// interpolation of the carrier phase at this rate keeps the S7 edge-time
// error inside its 4 ps budget for any video the 4fsc input can carry.
inline constexpr std::int64_t kModellingRateMultiple = 6;
inline constexpr double kModellingRateHz =
    static_cast<double>(kModellingRateMultiple) * ntsc::kSampleRateHz;
// D2 samples per frame: 6 × 477 750.
inline constexpr std::int64_t kModellingSamplesPerFrame =
    kModellingRateMultiple * ntsc::kSamplesPerFrame;

// Interpolation kernel (register S2.kernel): a Kaiser-windowed sinc whose
// cut-off is the input Nyquist frequency 2fsc,
//   k(x) = sinc(x) · w(x / kInterpolatorHalfWidth, kInterpolatorBeta),
// x in input sample periods. The window spans ±20 input samples, so each
// output sample is a weighted sum of 40 input samples.
inline constexpr std::size_t kInterpolatorHalfWidth = 20;
inline constexpr double kInterpolatorBeta = 9.0;

// The interpolation weights for output samples m ≡ phase (mod 6): weight j
// applies to input sample floor(u_m) − kInterpolatorHalfWidth + 1 + j, where
// u_m = (15·m − 57)/90 is output sample m's position in input samples.
// Each phase's weights are divided by their sum, so the DC gain is 1 to
// rounding.
using InterpolatorPhase = std::array<double, 2 * kInterpolatorHalfWidth>;
const std::array<InterpolatorPhase, kModellingRateMultiple>&
InterpolatorPhases();

// S2 video_rate_converter: D1 (IRE at 4fsc, origin 57/90 sample) → D2 (IRE
// at Fm, origin 0). Output sample m is x(m / Fm), where x is the
// band-limited signal of the input samples; the input is taken as 0 IRE
// (blanking) outside the run. The output has exactly 6 samples per input
// sample, so a run of F frames gives 2 866 500·F samples.
//
// O(40) multiply-adds per output sample. Block-size independent. Thread
// safety: one thread at a time.
class VideoRateConverter final : public IVideoRateConverter {
 public:
  VideoRateConverter();

  void Process(const IreSampleBlock& input, IreSampleBlock* output) override;
  void Finish(IreSampleBlock* output) override;

 private:
  // Emits every output sample whose inputs have all arrived, or, at the
  // end, every remaining output sample.
  void Emit(bool at_end, IreSampleBlock* output);

  const std::array<InterpolatorPhase, kModellingRateMultiple>& phases_;
  // Input samples from stream index history_first_ onwards.
  std::vector<double> history_;
  std::int64_t history_first_ = 0;
  // Input samples received so far.
  std::int64_t input_count_ = 0;
  // Next output sample to emit.
  std::int64_t next_output_ = 0;
};

}  // namespace ld_creator
