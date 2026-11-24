/*
 * File:        group_delay_predistorter.cpp
 * Module:      group_delay_predistorter
 * Purpose:     S3: IEC 60857 9.1.7 group-delay pre-distortion of the video
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/group_delay_predistorter/group_delay_predistorter.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <utility>

#include "ld_creator/dsp/fir_response.h"
#include "ld_creator/dsp/linear_solver.h"
#include "ld_creator/dsp/monotone_cubic.h"
#include "ld_creator/dsp/portable_math.h"
#include "ld_creator/standards/ntsc_timing.h"
#include "ld_creator/video_rate_converter/video_rate_converter.h"

namespace ld_creator {

namespace {

constexpr double kHzPerMhz = 1.0e6;
constexpr double kSecondsPerNs = 1.0e-9;
// ns · MHz = 10⁻³ (seconds × hertz = turns).
constexpr double kTurnsPerNsMhz = kSecondsPerNs * kHzPerMhz;

// Tap spacing: one 4fsc period, which is 6 Fm periods.
constexpr double kTapIntervalS = 1.0 / ntsc::kSampleRateHz;
constexpr std::int64_t kTapStride = kModellingRateMultiple;

// Ridge term, relative to the Gram matrix diagonal. It bounds the response
// in the 6 MHz–2fsc band the fit leaves free (the input has no content
// there), which otherwise makes the system nearly singular.
constexpr double kRidgeFraction = 1.0e-6;

MonotoneCubic DelayCurve(const std::vector<GroupDelayPoint>& table) {
  std::vector<double> x{0.0};
  std::vector<double> y{0.0};
  for (const GroupDelayPoint& point : table) {
    x.push_back(point.frequency_mhz);
    y.push_back(point.delay_ns);
  }
  return {std::move(x), std::move(y)};
}

}  // namespace

double TargetGroupDelayNs(const std::vector<GroupDelayPoint>& table,
                          double frequency_hz) {
  return DelayCurve(table).Value(frequency_hz / kHzPerMhz);
}

GroupDelayFilter DesignGroupDelayFilter(
    const std::vector<GroupDelayPoint>& table) {
  assert(!table.empty());
  const MonotoneCubic curve = DelayCurve(table);
  constexpr auto kTaps = static_cast<std::size_t>(kGroupDelayFilterTaps);

  // The realised delay is τ_c + t_d(f), with the constant τ_c placing the
  // spread of delays in the middle of the filter's span, rounded to whole
  // Fm periods so that removing it is an exact shift.
  double min_delay_ns = 0.0;
  for (const GroupDelayPoint& point : table) {
    min_delay_ns = std::min(min_delay_ns, point.delay_ns);
  }
  const double span_centre_s =
      (0.5 * static_cast<double>(kTaps - 1) * kTapIntervalS) -
      (0.5 * min_delay_ns * kSecondsPerNs);
  GroupDelayFilter filter;
  filter.advance_samples =
      static_cast<std::int64_t>(std::round(span_centre_s * kModellingRateHz));
  const double bulk_delay_s =
      static_cast<double>(filter.advance_samples) / kModellingRateHz;
  const double table_end_hz = table.back().frequency_mhz * kHzPerMhz;

  // Normal equations of min Σᵢ wᵢ |H(fᵢ) − D(fᵢ)|² over real taps cₖ, with
  // H(f) = Σ cₖ e^{−j2πfkT} and D(f) = e^{j2πθ(f)}:
  //   Σₗ cₗ Σᵢ wᵢ cos 2πfᵢ(k−l)T = Σᵢ wᵢ cos 2π(θ(fᵢ) + fᵢkT),
  // θ(f) = −(f·τ_c + ∫₀ᶠ t_d) in turns.
  const auto grid_points = static_cast<std::size_t>(
      std::llround(kGroupDelayDesignBandHz / kGroupDelayDesignStepHz));
  std::vector<double> gram_row(kTaps, 0.0);
  std::vector<double> rhs(kTaps, 0.0);
  for (std::size_t i = 0; i <= grid_points; ++i) {
    const double frequency = static_cast<double>(i) * kGroupDelayDesignStepHz;
    const double weight =
        frequency <= table_end_hz ? 1.0 : kGroupDelayOutOfTableWeight;
    const double target_turns =
        -((frequency * bulk_delay_s) +
          (kTurnsPerNsMhz * curve.Integral(frequency / kHzPerMhz)));
    for (std::size_t k = 0; k < kTaps; ++k) {
      const double tap_turns =
          frequency * static_cast<double>(k) * kTapIntervalS;
      gram_row[k] += weight * CosTurns(tap_turns);
      rhs[k] += weight * CosTurns(target_turns + tap_turns);
    }
  }
  SquareMatrix gram(kTaps);
  for (std::size_t row = 0; row < kTaps; ++row) {
    for (std::size_t column = 0; column < kTaps; ++column) {
      gram.At(row, column) =
          gram_row[row > column ? row - column : column - row];
    }
    gram.At(row, row) += kRidgeFraction * gram_row[0];
  }
  filter.taps = SolveSymmetricPositiveDefinite(std::move(gram), rhs);
  double sum = 0.0;
  for (const double tap : filter.taps) {
    sum += tap;
  }
  for (double& tap : filter.taps) {
    tap /= sum;
  }
  return filter;
}

double RealisedGroupDelaySeconds(const GroupDelayFilter& filter,
                                 double frequency_hz) {
  return FirResponseAt(filter.taps, kTapIntervalS, frequency_hz).group_delay_s -
         (static_cast<double>(filter.advance_samples) / kModellingRateHz);
}

double RealisedMagnitude(const GroupDelayFilter& filter, double frequency_hz) {
  return FirResponseAt(filter.taps, kTapIntervalS, frequency_hz).magnitude;
}

GroupDelayPredistorter::GroupDelayPredistorter(
    const GroupDelayPredistorterConfig& config)
    : enabled_(config.enabled) {
  if (enabled_) {
    filter_ = DesignGroupDelayFilter(config.delay_table);
  }
}

void GroupDelayPredistorter::Process(const IreSampleBlock& input,
                                     IreSampleBlock* output) {
  if (!enabled_) {
    *output = input;
    return;
  }
  history_.insert(history_.end(), input.ire.begin(), input.ire.end());
  input_count_ += static_cast<std::int64_t>(input.ire.size());
  Emit(/*at_end=*/false, output);
}

void GroupDelayPredistorter::Finish(IreSampleBlock* output) {
  if (!enabled_) {
    *output = IreSampleBlock{};
    return;
  }
  Emit(/*at_end=*/true, output);
}

void GroupDelayPredistorter::Emit(bool at_end, IreSampleBlock* output) {
  output->first_sample = next_output_;
  output->ire.clear();
  const auto taps = static_cast<std::int64_t>(filter_.taps.size());
  const std::int64_t reach_back = (taps - 1) * kTapStride;
  while (next_output_ < input_count_) {
    // Inputs used: newest m + advance, oldest m + advance − 6(N − 1).
    const std::int64_t newest = next_output_ + filter_.advance_samples;
    const std::int64_t oldest = newest - reach_back;
    if (!at_end && newest >= input_count_) {
      break;
    }
    double sum = 0.0;
    if (oldest >= history_first_ && newest < input_count_) {
      const double* samples = history_.data() + static_cast<std::ptrdiff_t>(
                                                    newest - history_first_);
      for (std::int64_t k = 0; k < taps; ++k) {
        sum += filter_.taps[static_cast<std::size_t>(k)] *
               samples[-static_cast<std::ptrdiff_t>(k * kTapStride)];
      }
    } else {
      // At either end of the run: samples outside it are 0 IRE.
      for (std::int64_t k = 0; k < taps; ++k) {
        const std::int64_t index = newest - (k * kTapStride);
        if (index >= 0 && index < input_count_) {
          sum += filter_.taps[static_cast<std::size_t>(k)] *
                 history_[static_cast<std::size_t>(index - history_first_)];
        }
      }
    }
    output->ire.push_back(sum);
    ++next_output_;
  }
  const std::int64_t keep_from = std::max<std::int64_t>(
      0, next_output_ + filter_.advance_samples - reach_back);
  if (keep_from > history_first_) {
    const std::int64_t drop = std::min<std::int64_t>(
        keep_from - history_first_, static_cast<std::int64_t>(history_.size()));
    history_.erase(history_.begin(),
                   history_.begin() + static_cast<std::ptrdiff_t>(drop));
    history_first_ += drop;
  }
}

}  // namespace ld_creator
