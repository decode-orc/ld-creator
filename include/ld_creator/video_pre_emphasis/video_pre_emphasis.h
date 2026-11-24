/*
 * File:        video_pre_emphasis.h
 * Module:      video_pre_emphasis
 * Purpose:     S4: IEC 60857 9.2.4 video pre-emphasis at the modelling rate
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include "ld_creator/pipeline/stage_interfaces.h"
#include "ld_creator/project/project_config.h"

namespace ld_creator {

// The discretised pre-emphasis H(s) = (1 + s·t₁)/(1 + s·t₂) (IEC 60857-1986
// 9.2.4), written as H = 1 + (t₁/t₂ − 1)·s·t₂/(1 + s·t₂) and discretised by
// the bilinear transform s = 2·Fm·(1 − z⁻¹)/(1 + z⁻¹) without pre-warping
// (S4 note):
//   hp[m] = gain·(x[m] − x[m−1]) − pole·hp[m−1]
//   y[m]  = x[m] + shelf·hp[m]
// The high-pass branch sees only differences of the input, so a constant
// input passes exactly unchanged: the DC gain is exactly 1.
struct PreEmphasisCoefficients {
  double gain = 0.0;   // K·t₂ / (1 + K·t₂), K = 2·Fm
  double pole = 0.0;   // (1 − K·t₂) / (1 + K·t₂)
  double shelf = 0.0;  // t₁/t₂ − 1
};

PreEmphasisCoefficients DesignPreEmphasis(const VideoPreEmphasisConfig& config);

// |H(e^{j2πf/Fm})| of the discretised filter. Pure function.
double DiscretePreEmphasisMagnitude(const PreEmphasisCoefficients& coefficients,
                                    double frequency_hz);

// |H(j2πf)| of the analogue §9.2.4 response. Pure function.
double AnaloguePreEmphasisMagnitude(const VideoPreEmphasisConfig& config,
                                    double frequency_hz);

// S4 video_pre_emphasis: D2 → D2, same length and time origin. The filter
// starts at rest with the input 0 IRE before the run. Its own group delay is
// part of the specified response and is not removed (S4 note).
//
// O(1) per sample. Block-size independent. Thread safety: one thread at a
// time.
class VideoPreEmphasis final : public IVideoPreEmphasis {
 public:
  explicit VideoPreEmphasis(const VideoPreEmphasisConfig& config);

  void Process(const IreSampleBlock& input, IreSampleBlock* output) override;
  void Finish(IreSampleBlock* output) override;

 private:
  PreEmphasisCoefficients coefficients_;
  double previous_input_ = 0.0;
  double high_pass_ = 0.0;
};

}  // namespace ld_creator
