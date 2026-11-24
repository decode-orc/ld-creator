/*
 * File:        video_rate_converter.cpp
 * Module:      video_rate_converter
 * Purpose:     S2: band-limited interpolation of 4fsc video to the modelling
 * rate
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/video_rate_converter/video_rate_converter.h"

#include <algorithm>
#include <cstddef>

#include "ld_creator/dsp/kaiser_window.h"
#include "ld_creator/dsp/portable_math.h"

namespace ld_creator {

namespace {

// Output sample m sits at u_m = (15·m − 57)/90 input samples: t_m = m / Fm
// and input sample n is at (n + 57/90) / 4fsc, so u_m = m/6 − 57/90.
constexpr std::int64_t kPositionNumeratorPerOutput = 15;
constexpr std::int64_t kPositionOffsetNumerator = 57;
constexpr std::int64_t kPositionDenominator = 90;

constexpr auto kHalfWidth = static_cast<std::int64_t>(kInterpolatorHalfWidth);
constexpr std::int64_t kTaps = 2 * kHalfWidth;

constexpr double kPi = kTwoPi / 2.0;

std::int64_t FloorDivide(std::int64_t numerator, std::int64_t denominator) {
  std::int64_t quotient = numerator / denominator;
  if ((numerator % denominator != 0) &&
      ((numerator < 0) != (denominator < 0))) {
    --quotient;
  }
  return quotient;
}

// Index of the input sample at or before output sample m's position, and the
// remaining fraction's numerator (over 90).
struct OutputPosition {
  std::int64_t base_input = 0;
  std::int64_t fraction_numerator = 0;
};

OutputPosition PositionOf(std::int64_t output) {
  const std::int64_t numerator =
      (kPositionNumeratorPerOutput * output) - kPositionOffsetNumerator;
  const std::int64_t base = FloorDivide(numerator, kPositionDenominator);
  return {.base_input = base,
          .fraction_numerator = numerator - (base * kPositionDenominator)};
}

// sinc(x) = sin(πx)/(πx), x ≠ 0.
double Sinc(double x) { return SinTurns(0.5 * x) / (kPi * x); }

std::array<InterpolatorPhase, kModellingRateMultiple> BuildPhases() {
  std::array<InterpolatorPhase, kModellingRateMultiple> phases{};
  for (std::int64_t phase = 0; phase < kModellingRateMultiple; ++phase) {
    const OutputPosition position = PositionOf(phase);
    const double fraction = static_cast<double>(position.fraction_numerator) /
                            static_cast<double>(kPositionDenominator);
    InterpolatorPhase& weights = phases[static_cast<std::size_t>(phase)];
    double sum = 0.0;
    for (std::int64_t j = 0; j < kTaps; ++j) {
      // Distance from output position to input base − H + 1 + j.
      const double x = fraction + static_cast<double>(kHalfWidth - 1 - j);
      const double weight =
          Sinc(x) *
          KaiserWindow(x / static_cast<double>(kHalfWidth), kInterpolatorBeta);
      weights[static_cast<std::size_t>(j)] = weight;
      sum += weight;
    }
    for (double& weight : weights) {
      weight /= sum;
    }
  }
  return phases;
}

}  // namespace

const std::array<InterpolatorPhase, kModellingRateMultiple>&
InterpolatorPhases() {
  static const std::array<InterpolatorPhase, kModellingRateMultiple> phases =
      BuildPhases();
  return phases;
}

VideoRateConverter::VideoRateConverter() : phases_(InterpolatorPhases()) {}

void VideoRateConverter::Process(const IreSampleBlock& input,
                                 IreSampleBlock* output) {
  history_.insert(history_.end(), input.ire.begin(), input.ire.end());
  input_count_ += static_cast<std::int64_t>(input.ire.size());
  Emit(/*at_end=*/false, output);
}

void VideoRateConverter::Finish(IreSampleBlock* output) {
  Emit(/*at_end=*/true, output);
}

void VideoRateConverter::Emit(bool at_end, IreSampleBlock* output) {
  output->first_sample = next_output_;
  output->ire.clear();
  const std::int64_t output_end = kModellingRateMultiple * input_count_;
  while (next_output_ < output_end) {
    const OutputPosition position = PositionOf(next_output_);
    const std::int64_t first_input = position.base_input - kHalfWidth + 1;
    const std::int64_t last_input = position.base_input + kHalfWidth;
    if (!at_end && last_input >= input_count_) {
      break;
    }
    const InterpolatorPhase& weights = phases_[static_cast<std::size_t>(
        next_output_ % kModellingRateMultiple)];
    double sum = 0.0;
    if (first_input >= history_first_ && last_input < input_count_) {
      const double* samples =
          history_.data() +
          static_cast<std::ptrdiff_t>(first_input - history_first_);
      for (std::size_t j = 0; j < weights.size(); ++j) {
        sum += weights[j] * samples[j];
      }
    } else {
      // At either end of the run: samples outside it are 0 IRE.
      for (std::int64_t j = 0; j < kTaps; ++j) {
        const std::int64_t index = first_input + j;
        if (index >= 0 && index < input_count_) {
          sum += weights[static_cast<std::size_t>(j)] *
                 history_[static_cast<std::size_t>(index - history_first_)];
        }
      }
    }
    output->ire.push_back(sum);
    ++next_output_;
  }
  // Keep only what the next output sample needs.
  const std::int64_t keep_from = std::max<std::int64_t>(
      0, PositionOf(next_output_).base_input - kHalfWidth + 1);
  if (keep_from > history_first_) {
    const std::int64_t drop = std::min<std::int64_t>(
        keep_from - history_first_, static_cast<std::int64_t>(history_.size()));
    history_.erase(history_.begin(),
                   history_.begin() + static_cast<std::ptrdiff_t>(drop));
    history_first_ += drop;
  }
}

}  // namespace ld_creator
