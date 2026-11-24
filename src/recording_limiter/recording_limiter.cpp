/*
 * File:        recording_limiter.cpp
 * Module:      recording_limiter
 * Purpose:     S7: pit-edge times where the limited carrier changes level
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/recording_limiter/recording_limiter.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>

#include "ld_creator/dsp/hermite_phase.h"
#include "ld_creator/dsp/portable_math.h"
#include "ld_creator/video_rate_converter/video_rate_converter.h"

namespace ld_creator {

namespace {

constexpr double kIntervalS = 1.0 / kModellingRateHz;

// A Newton step this small in u means the root is found to the resolution
// of a double near 1.
constexpr double kConvergedStep = 2.0 * std::numeric_limits<double>::epsilon();

// Recording time of a point `u` of the way through interval `index`.
double TimeAt(std::int64_t index, double u) {
  return (static_cast<double>(index) + u) / kModellingRateHz;
}

// The phase of crossing k, (k + ½)π. Every interval evaluates it the same
// way, so a crossing that falls exactly on a sample belongs to one interval.
double TargetPhase(std::int64_t k) {
  return (static_cast<double>(k) + 0.5) * std::numbers::pi;
}

// d/du of HermitePhaseIncrement (hermite_phase.h).
double HermitePhaseSlope(double phase_step_rad, double frequency0_hz,
                         double frequency1_hz, double u) {
  const double u2 = u * u;
  const double h01 = (6.0 * u) - (6.0 * u2);
  const double h10 = (3.0 * u2) - (4.0 * u) + 1.0;
  const double h11 = (3.0 * u2) - (2.0 * u);
  return (h01 * phase_step_rad) +
         (kIntervalS * kTwoPi *
          ((h10 * frequency0_hz) + (h11 * frequency1_hz)));
}

// The fraction u ∈ (0, 1] of the interval at which the interpolant has risen
// by `rise_rad` ∈ (0, phase_step_rad]. The interpolant is monotone, so the
// root is unique; Newton's method converges quadratically and the bracket
// keeps every iterate inside [0, 1].
double SolveCrossing(double phase_step_rad, double frequency0_hz,
                     double frequency1_hz, double rise_rad) {
  double low = 0.0;
  double high = 1.0;
  double u = rise_rad / phase_step_rad;
  for (int iteration = 0; iteration < kMaximumEdgeIterations; ++iteration) {
    const double residual =
        HermitePhaseIncrement(phase_step_rad, frequency0_hz, frequency1_hz,
                              kIntervalS, u) -
        rise_rad;
    if (residual == 0.0) {
      return u;
    }
    if (residual < 0.0) {
      low = u;
    } else {
      high = u;
    }
    const double slope =
        HermitePhaseSlope(phase_step_rad, frequency0_hz, frequency1_hz, u);
    double next = u - (residual / slope);
    if (!(next > low && next < high)) {
      next = 0.5 * (low + high);
    }
    // Converged: the step is at the resolution of u, which is below
    // 10⁻²³ s of edge time.
    if (std::abs(next - u) <= kConvergedStep) {
      return next;
    }
    u = next;
  }
  return u;
}

}  // namespace

void RecordingLimiter::Process(const CarrierPhaseBlock& input,
                               RecordedEdgeBlock* output) {
  assert(input.frequency_hz.size() == input.phase_rad.size());
  assert(input.slice_level.size() == input.phase_rad.size());
  output->edges.clear();
  for (std::size_t i = 0; i < input.phase_rad.size(); ++i) {
    if (input.slice_level[i] != 0.0) {
      throw std::invalid_argument(
          "recording_limiter: a non-zero slice level (audio or EFM) is not "
          "implemented");
    }
    const std::int64_t index =
        input.first_sample + static_cast<std::int64_t>(i);
    if (!started_) {
      // The first crossing is the first (k + ½)π above φ at sample 0.
      const double phase = input.phase_rad[i];
      next_k_ = static_cast<std::int64_t>(std::floor(phase / std::numbers::pi));
      while (TargetPhase(next_k_) <= phase) {
        ++next_k_;
      }
      while (TargetPhase(next_k_ - 1) > phase) {
        --next_k_;
      }
      started_ = true;
    } else {
      assert(index == previous_index_ + 1);
      CutInterval(input.phase_rad[i], input.frequency_hz[i], output);
    }
    previous_index_ = index;
    previous_phase_rad_ = input.phase_rad[i];
    previous_frequency_hz_ = input.frequency_hz[i];
  }
  output->complete_until_s = started_ ? TimeAt(previous_index_, 0.0) : 0.0;
}

void RecordingLimiter::Finish(RecordedEdgeBlock* output) {
  output->edges.clear();
  output->complete_until_s = started_ ? TimeAt(previous_index_, 0.0) : 0.0;
}

void RecordingLimiter::CutInterval(double phase1_rad, double frequency1_hz,
                                   RecordedEdgeBlock* output) {
  const double phase0 = previous_phase_rad_;
  const double frequency0 = previous_frequency_hz_;
  const double step = phase1_rad - phase0;
  // Monotone interpolant: positive secant, end slopes within (0, 3]·secant.
  const double ratio0 = kTwoPi * frequency0 * kIntervalS / step;
  const double ratio1 = kTwoPi * frequency1_hz * kIntervalS / step;
  if (!(step > 0.0) || !(ratio0 > 0.0 && ratio0 <= kMonotoneSlopeRatio) ||
      !(ratio1 > 0.0 && ratio1 <= kMonotoneSlopeRatio)) {
    throw std::runtime_error(
        "recording_limiter: carrier phase is not monotonic after D3 sample " +
        std::to_string(previous_index_));
  }
  while (TargetPhase(next_k_) <= phase1_rad) {
    const double u = SolveCrossing(step, frequency0, frequency1_hz,
                                   TargetPhase(next_k_) - phase0);
    // cos φ falls through zero at even k, ending the pit (cos φ > 0).
    const EdgePolarity polarity =
        (next_k_ % 2 == 0) ? EdgePolarity::kPitEnd : EdgePolarity::kPitStart;
    output->edges.push_back(RecordedEdge{.time_s = TimeAt(previous_index_, u),
                                         .polarity = polarity});
    ++next_k_;
  }
}

}  // namespace ld_creator
