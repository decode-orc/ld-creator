/*
 * File:        portable_math.h
 * Module:      dsp
 * Purpose:     Sine and cosine built from IEEE 754 basic operations only
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

namespace ld_creator {

// 2π, rounded to double.
inline constexpr double kTwoPi = 6.283185307179586;

struct SinCosValue {
  double sin = 0.0;
  double cos = 1.0;
};

// sin and cos of 2π·turns.
//
// Why not std::sin: the C library's transcendental functions are not
// required to be correctly rounded, and their last bit differs between
// libraries. The model's output must be byte-identical on every host
// (docs-tech/design/high-level-design.md §2.3), so every value that reaches
// the RF is computed from + − × ÷ and rounding to an integer, which IEEE 754
// defines exactly.
//
// Method: subtract the nearest whole turn and then the nearest quarter turn
// (both exact by Sterbenz's lemma), leaving |s| ≤ 1/8 turn; evaluate the
// Taylor series of sin and cos of 2πs to degree 17 and 18, whose truncation
// error is below 1e-19 for |2πs| ≤ π/4; then rotate by the quarter turns.
// Within 6e-16 of the true value for the stored argument (unit tests), for
// |turns| < 2^52. Pure function, O(1).
SinCosValue SinCosTurns(double turns);

// cos(2π·turns) and sin(2π·turns); see SinCosTurns.
double CosTurns(double turns);
double SinTurns(double turns);

// sin and cos of an angle in radians: SinCosTurns(radians / 2π). The
// division adds a relative error of 1.1e-16 to the angle.
SinCosValue SinCosRadians(double radians);

}  // namespace ld_creator
