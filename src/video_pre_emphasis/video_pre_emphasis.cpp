/*
 * File:        video_pre_emphasis.cpp
 * Module:      video_pre_emphasis
 * Purpose:     S4: IEC 60857 9.2.4 video pre-emphasis at the modelling rate
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/video_pre_emphasis/video_pre_emphasis.h"

#include <cmath>

#include "ld_creator/dsp/portable_math.h"
#include "ld_creator/video_rate_converter/video_rate_converter.h"

namespace ld_creator {

namespace {

constexpr double kSecondsPerNs = 1.0e-9;

}  // namespace

PreEmphasisCoefficients DesignPreEmphasis(
    const VideoPreEmphasisConfig& config) {
  const double t1 = config.t1_ns * kSecondsPerNs;
  const double t2 = config.t2_ns * kSecondsPerNs;
  const double k_t2 = 2.0 * kModellingRateHz * t2;
  return {.gain = k_t2 / (1.0 + k_t2),
          .pole = (1.0 - k_t2) / (1.0 + k_t2),
          .shelf = (t1 / t2) - 1.0};
}

double DiscretePreEmphasisMagnitude(const PreEmphasisCoefficients& coefficients,
                                    double frequency_hz) {
  // HP(z) = gain·(1 − z⁻¹)/(1 + pole·z⁻¹), H = 1 + shelf·HP, z⁻¹ = e^{−jω/Fm}.
  const SinCosValue z = SinCosTurns(-frequency_hz / kModellingRateHz);
  const double num_re = coefficients.gain * (1.0 - z.cos);
  const double num_im = -coefficients.gain * z.sin;
  const double den_re = 1.0 + (coefficients.pole * z.cos);
  const double den_im = coefficients.pole * z.sin;
  const double den_power = (den_re * den_re) + (den_im * den_im);
  const double hp_re = ((num_re * den_re) + (num_im * den_im)) / den_power;
  const double hp_im = ((num_im * den_re) - (num_re * den_im)) / den_power;
  const double h_re = 1.0 + (coefficients.shelf * hp_re);
  const double h_im = coefficients.shelf * hp_im;
  return std::sqrt((h_re * h_re) + (h_im * h_im));
}

double AnaloguePreEmphasisMagnitude(const VideoPreEmphasisConfig& config,
                                    double frequency_hz) {
  const double omega = kTwoPi * frequency_hz;
  const double w1 = omega * config.t1_ns * kSecondsPerNs;
  const double w2 = omega * config.t2_ns * kSecondsPerNs;
  return std::sqrt((1.0 + (w1 * w1)) / (1.0 + (w2 * w2)));
}

VideoPreEmphasis::VideoPreEmphasis(const VideoPreEmphasisConfig& config)
    : coefficients_(DesignPreEmphasis(config)) {}

void VideoPreEmphasis::Process(const IreSampleBlock& input,
                               IreSampleBlock* output) {
  output->first_sample = input.first_sample;
  output->ire.resize(input.ire.size());
  for (std::size_t m = 0; m < input.ire.size(); ++m) {
    const double x = input.ire[m];
    high_pass_ = (coefficients_.gain * (x - previous_input_)) -
                 (coefficients_.pole * high_pass_);
    previous_input_ = x;
    output->ire[m] = x + (coefficients_.shelf * high_pass_);
  }
}

void VideoPreEmphasis::Finish(IreSampleBlock* output) {
  *output = IreSampleBlock{};
}

}  // namespace ld_creator
