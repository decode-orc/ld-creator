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

// e^x, by the same rule: x = k·ln 2 + r with ln 2 split into a high part
// whose product with k is exact and a low part (Cody & Waite), |r| ≤ ½ ln 2;
// Taylor series of e^r to degree 20 (truncation below 1e-20); scaled by 2^k
// with std::ldexp, which is exact. Within 4 ulp of the true value (unit
// tests). Returns 0 below −745 and +∞ above 709.7. Pure function, O(1).
double PortableExp(double x);

// e^x − 1 without cancellation: the Taylor series directly for
// |x| ≤ ½ ln 2, PortableExp(x) − 1 otherwise. Pure function, O(1).
double PortableExpm1(double x);

// Natural logarithm for x > 0: x = m·2^e with m ∈ [√½, √2) (std::frexp,
// exact), ln m = 2·atanh(z), z = (m − 1)/(m + 1), |z| ≤ 0.172, by its odd
// series to z^41; plus e·ln 2 in two parts. Within 4 ulp (unit tests).
// Returns NaN for x ≤ 0. Pure function, O(1).
double PortableLog(double x);

// The error function. For |x| ≤ 2.5, from the series with positive terms
//   erf x = (2/√π)·e^(−x²)·Σₙ 2ⁿ·x^(2n+1) / (1·3·5···(2n+1))
// summed until a term no longer changes the sum (Abramowitz & Stegun
// 7.1.6), which has no cancellation; above, 1 − erfc x from its continued
// fraction (7.1.14) to 60 terms; ±1 beyond |x| = 6. Within 1e-15 of the
// true value (unit tests). Odd in x. Pure function.
double PortableErf(double x);

// arctan x: reduced to |x| ≤ 1 by arctan x = π/2 − arctan(1/x), then halved
// twice with arctan x = 2·arctan(x / (1 + √(1 + x²))) to |x| ≤ tan(π/16),
// then its odd Taylor series to x^39. Within 4 ulp (unit tests). Pure
// function, O(1).
double PortableAtan(double x);

// A smooth step from 1 at s ≤ 0 to 0 at s ≥ 1, infinitely differentiable:
// 1 − ψ(s), ψ(s) = e^(−1/s) / (e^(−1/s) + e^(−1/(1−s))). Its Fourier
// transform decays faster than any power, unlike a raised cosine (whose
// second derivative jumps, so its transform falls only as f⁻³). Used to
// taper responses and kernels. Pure function, O(1).
double SmoothStepDown(double s);

// arccos y for |y| ≤ 1: 2·arctan √((1 − y)/(1 + y)), which keeps full
// relative accuracy as y → 1. Pure function, O(1).
double PortableAcos(double y);

}  // namespace ld_creator
