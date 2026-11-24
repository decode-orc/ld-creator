/*
 * File:        portable_math.cpp
 * Module:      dsp
 * Purpose:     Sine and cosine built from IEEE 754 basic operations only
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/dsp/portable_math.h"

#include <array>
#include <cmath>
#include <cstddef>

namespace ld_creator {

namespace {

// Highest Taylor power used: sin to x^17, cos to x^18.
constexpr std::size_t kSeriesTerms = 19;

// 1/n! for n = 0 … 18, computed at compile time by repeated division, so the
// values are the same on every host.
constexpr std::array<double, kSeriesTerms> InverseFactorials() {
  std::array<double, kSeriesTerms> values{};
  values[0] = 1.0;
  for (std::size_t n = 1; n < kSeriesTerms; ++n) {
    values[n] = values[n - 1] / static_cast<double>(n);
  }
  return values;
}

constexpr std::array<double, kSeriesTerms> kInverseFactorial =
    InverseFactorials();

// Taylor series of sin x and cos x for |x| ≤ π/4, by Horner's rule in x².
SinCosValue SeriesSinCos(double x) {
  const double x2 = x * x;
  double sin_sum = 0.0;
  for (std::size_t n = kSeriesTerms - 2; n >= 3; n -= 2) {
    sin_sum = (n % 4 == 1 ? kInverseFactorial[n] : -kInverseFactorial[n]) +
              x2 * sin_sum;
  }
  sin_sum = x * (1.0 + x2 * sin_sum);
  double cos_sum = 0.0;
  for (std::size_t n = kSeriesTerms - 1; n >= 2; n -= 2) {
    cos_sum = (n % 4 == 0 ? kInverseFactorial[n] : -kInverseFactorial[n]) +
              x2 * cos_sum;
  }
  cos_sum = 1.0 + x2 * cos_sum;
  return {.sin = sin_sum, .cos = cos_sum};
}

}  // namespace

SinCosValue SinCosTurns(double turns) {
  // Both reductions are exact: each subtracts a nearby value (Sterbenz).
  const double whole = std::round(turns);
  const double fraction = turns - whole;
  const double quarters = std::round(4.0 * fraction);
  const double remainder = fraction - 0.25 * quarters;
  const SinCosValue base = SeriesSinCos(kTwoPi * remainder);
  // quarters ∈ {−2, …, 2}; rotate by quarters × 90°.
  const int quadrant = (static_cast<int>(quarters) + 4) % 4;
  switch (quadrant) {
    case 1:
      return {.sin = base.cos, .cos = -base.sin};
    case 2:
      return {.sin = -base.sin, .cos = -base.cos};
    case 3:
      return {.sin = -base.cos, .cos = base.sin};
    default:
      return base;
  }
}

double CosTurns(double turns) { return SinCosTurns(turns).cos; }

double SinTurns(double turns) { return SinCosTurns(turns).sin; }

SinCosValue SinCosRadians(double radians) {
  return SinCosTurns(radians / kTwoPi);
}

}  // namespace ld_creator
