/*
 * File:        sinusoid_fit.h
 * Module:      tests
 * Purpose:     Least-squares sinusoid fit for measuring gain and delay in tests
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>

namespace ld_creator::test {

// y ≈ amplitude · cos(2πf·t − phase), fitted by least squares over samples
// y[i] at times t[i].
struct SinusoidFit {
  double amplitude = 0.0;
  double phase_rad = 0.0;
};

inline SinusoidFit FitSinusoid(std::span<const double> values,
                               std::span<const double> times,
                               double frequency_hz) {
  // Normal equations for y = a·cos ωt + b·sin ωt.
  double cc = 0.0;
  double cs = 0.0;
  double ss = 0.0;
  double yc = 0.0;
  double ys = 0.0;
  for (std::size_t i = 0; i < values.size(); ++i) {
    const double angle = 2.0 * std::numbers::pi * frequency_hz * times[i];
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    cc += c * c;
    cs += c * s;
    ss += s * s;
    yc += values[i] * c;
    ys += values[i] * s;
  }
  const double det = (cc * ss) - (cs * cs);
  const double a = ((yc * ss) - (ys * cs)) / det;
  const double b = ((ys * cc) - (yc * cs)) / det;
  return {.amplitude = std::hypot(a, b), .phase_rad = std::atan2(b, a)};
}

// The delay, in seconds, of `fit` relative to `reference` at `frequency_hz`,
// with the phase difference wrapped to (−π, π].
inline double DelayBetween(const SinusoidFit& fit, const SinusoidFit& reference,
                           double frequency_hz) {
  double difference = fit.phase_rad - reference.phase_rad;
  while (difference > std::numbers::pi) {
    difference -= 2.0 * std::numbers::pi;
  }
  while (difference <= -std::numbers::pi) {
    difference += 2.0 * std::numbers::pi;
  }
  return difference / (2.0 * std::numbers::pi * frequency_hz);
}

}  // namespace ld_creator::test
