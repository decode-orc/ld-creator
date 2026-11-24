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
#include <limits>
#include <numbers>

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

namespace {

// ln 2 = kLn2High + kLn2Low. kLn2High has its low 11 bits clear, so k·kLn2High
// is exact for |k| < 2^11 (Cody & Waite).
constexpr double kLn2High = 6.93147180369123816490e-01;
constexpr double kLn2Low = 1.90821492927058770002e-10;
constexpr double kLn2 = 0.6931471805599453;
constexpr double kExpUnderflow = -745.2;
constexpr double kExpOverflow = 709.78;
constexpr std::size_t kExpSeriesDegree = 20;

constexpr std::array<double, kExpSeriesDegree + 1> ExpCoefficients() {
  std::array<double, kExpSeriesDegree + 1> values{};
  values[0] = 1.0;
  for (std::size_t n = 1; n < values.size(); ++n) {
    values[n] = values[n - 1] / static_cast<double>(n);
  }
  return values;
}

constexpr std::array<double, kExpSeriesDegree + 1> kExpCoefficient =
    ExpCoefficients();

// e^r − 1 for |r| ≤ ½ ln 2, Taylor series by Horner's rule.
double SeriesExpm1(double r) {
  double sum = 0.0;
  for (std::size_t n = kExpSeriesDegree; n >= 1; --n) {
    sum = kExpCoefficient[n] + r * sum;
  }
  return r * sum;
}

}  // namespace

double PortableExp(double x) {
  if (x < kExpUnderflow) {
    return 0.0;
  }
  if (x > kExpOverflow) {
    return std::numeric_limits<double>::infinity();
  }
  const double k = std::round(x / kLn2);
  const double r = (x - k * kLn2High) - k * kLn2Low;
  return std::ldexp(1.0 + SeriesExpm1(r), static_cast<int>(k));
}

double PortableExpm1(double x) {
  if (std::abs(x) <= 0.5 * kLn2) {
    return SeriesExpm1(x);
  }
  return PortableExp(x) - 1.0;
}

double PortableLog(double x) {
  if (!(x > 0.0)) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  if (std::isinf(x)) {
    return x;
  }
  int exponent = 0;
  double mantissa = std::frexp(x, &exponent);  // [0.5, 1)
  if (mantissa < std::numbers::sqrt2 / 2.0) {
    mantissa *= 2.0;
    --exponent;
  }
  const double z = (mantissa - 1.0) / (mantissa + 1.0);
  const double z2 = z * z;
  // 2·atanh z = 2·Σ z^(2k+1)/(2k+1), k = 0 … 20.
  constexpr int kLogSeriesTerms = 21;
  double sum = 0.0;
  for (int k = kLogSeriesTerms - 1; k >= 0; --k) {
    sum = 1.0 / static_cast<double>(2 * k + 1) + z2 * sum;
  }
  const double e = static_cast<double>(exponent);
  return (e * kLn2High) + ((2.0 * z * sum) + (e * kLn2Low));
}

double PortableErf(double x) {
  constexpr double kErfSaturation = 6.0;
  constexpr double kSeriesLimit = 2.5;
  // 1/√π.
  constexpr double kInverseRootPi = 0.5641895835477563;
  const double magnitude = std::abs(x);
  double value = 1.0;
  if (magnitude > kErfSaturation) {
    value = 1.0;
  } else if (magnitude <= kSeriesLimit) {
    const double x2 = magnitude * magnitude;
    double term = magnitude;
    double sum = magnitude;
    for (int n = 1;; ++n) {
      term *= 2.0 * x2 / static_cast<double>(2 * n + 1);
      const double next = sum + term;
      if (next == sum) {
        break;
      }
      sum = next;
    }
    value = 2.0 * kInverseRootPi * PortableExp(-x2) * sum;
  } else {
    // erfc x = e^(−x²)/√π · 1/(x + ½/(x + 1/(x + (3/2)/(x + …)))),
    // evaluated from the tail (Abramowitz & Stegun 7.1.14).
    constexpr int kFractionTerms = 60;
    double tail = magnitude;
    for (int n = kFractionTerms; n >= 1; --n) {
      tail = magnitude + (0.5 * static_cast<double>(n)) / tail;
    }
    value = 1.0 - (kInverseRootPi * PortableExp(-magnitude * magnitude) / tail);
  }
  return x < 0.0 ? -value : value;
}

double PortableAtan(double x) {
  const double magnitude = std::abs(x);
  const bool inverted = magnitude > 1.0;
  double t = inverted ? 1.0 / magnitude : magnitude;
  // Two halvings: |t| ≤ tan(π/16) ≈ 0.199.
  constexpr int kHalvings = 2;
  for (int i = 0; i < kHalvings; ++i) {
    t = t / (1.0 + std::sqrt(1.0 + (t * t)));
  }
  const double t2 = t * t;
  constexpr int kAtanSeriesTerms = 20;
  double sum = 0.0;
  for (int k = kAtanSeriesTerms - 1; k >= 0; --k) {
    const double coefficient = 1.0 / static_cast<double>(2 * k + 1);
    sum = (k % 2 == 0 ? coefficient : -coefficient) + t2 * sum;
  }
  double value = 4.0 * t * sum;
  if (inverted) {
    value = (0.5 * std::numbers::pi) - value;
  }
  return x < 0.0 ? -value : value;
}

double SmoothStepDown(double s) {
  if (s <= 0.0) {
    return 1.0;
  }
  if (s >= 1.0) {
    return 0.0;
  }
  const double rising = PortableExp(-1.0 / s);
  const double falling = PortableExp(-1.0 / (1.0 - s));
  return falling / (rising + falling);
}

double PortableAcos(double y) {
  return 2.0 * PortableAtan(std::sqrt((1.0 - y) / (1.0 + y)));
}

}  // namespace ld_creator
