/*
 * File:        hermite_phase.h
 * Module:      dsp
 * Purpose:     Cubic Hermite interpolation of carrier phase between samples
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include "ld_creator/dsp/portable_math.h"

namespace ld_creator {

// The phase increment from sample 0 to fraction u ∈ [0, 1] of an interval of
// `interval_s` seconds, from the cubic Hermite interpolant of φ with end
// values φ₀, φ₁ and end slopes 2π·f₀, 2π·f₁ (S5 note; error bound in the
// S2 note). Returned relative to φ₀ so that the large absolute phase does
// not cancel:
//   φ(u) − φ₀ = h01(u)·(φ₁ − φ₀) + interval·2π·(h10(u)·f₀ + h11(u)·f₁)
// with h01 = 3u² − 2u³, h10 = u³ − 2u² + u, h11 = u³ − u². Pure function.
inline double HermitePhaseIncrement(double phase_step_rad, double frequency0_hz,
                                    double frequency1_hz, double interval_s,
                                    double u) {
  const double u2 = u * u;
  const double u3 = u2 * u;
  const double h01 = (3.0 * u2) - (2.0 * u3);
  const double h10 = u3 - (2.0 * u2) + u;
  const double h11 = u3 - u2;
  return (h01 * phase_step_rad) +
         (interval_s * kTwoPi *
          ((h10 * frequency0_hz) + (h11 * frequency1_hz)));
}

}  // namespace ld_creator
