/*
 * File:        test_dsp_numerics.cpp
 * Module:      tests
 * Purpose:     Unit tests for portable functions, FFT, quadrature, tables
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <vector>

#include "ld_creator/dsp/complex_value.h"
#include "ld_creator/dsp/edge_response_table.h"
#include "ld_creator/dsp/fft.h"
#include "ld_creator/dsp/gauss_legendre.h"
#include "ld_creator/dsp/portable_math.h"

namespace ld_creator {
namespace {

// |a − b| in units of the spacing of doubles at b.
double UlpDistance(double a, double b) {
  const double ulp =
      std::nextafter(std::abs(b), std::numeric_limits<double>::infinity()) -
      std::abs(b);
  return std::abs(a - b) / ulp;
}

TEST(PortableFunctionsTest, ExpIsWithinFourUlpOfTheLibrary) {
  for (double x = -700.0; x <= 700.0; x += 0.37) {
    EXPECT_LE(UlpDistance(PortableExp(x), std::exp(x)), 4.0) << x;
  }
  for (double x = -1.0; x <= 1.0; x += 1.0 / 1024.0) {
    EXPECT_LE(UlpDistance(PortableExp(x), std::exp(x)), 4.0) << x;
  }
  EXPECT_EQ(PortableExp(0.0), 1.0);
  EXPECT_EQ(PortableExp(-800.0), 0.0);
}

TEST(PortableFunctionsTest, Expm1KeepsRelativeAccuracyNearZero) {
  for (double x = -0.34; x <= 0.34; x += 1.0 / 4096.0) {
    if (x != 0.0) {
      EXPECT_LE(UlpDistance(PortableExpm1(x), std::expm1(x)), 4.0) << x;
    }
  }
  EXPECT_LE(UlpDistance(PortableExpm1(1e-12), std::expm1(1e-12)), 4.0);
}

TEST(PortableFunctionsTest, LogIsWithinFourUlpOfTheLibrary) {
  for (double x = 1e-300; x < 1e300; x *= 1.37) {
    EXPECT_LE(UlpDistance(PortableLog(x), std::log(x)), 4.0) << x;
  }
  for (double x = 0.5; x <= 2.0; x += 1.0 / 1024.0) {
    if (x != 1.0) {
      EXPECT_LE(std::abs(PortableLog(x) - std::log(x)),
                4.0 * std::numeric_limits<double>::epsilon())
          << x;
    }
  }
  EXPECT_EQ(PortableLog(1.0), 0.0);
  EXPECT_TRUE(std::isnan(PortableLog(-1.0)));
}

TEST(PortableFunctionsTest, ErfMatchesTheLibrary) {
  for (double x = -6.0; x <= 6.0; x += 1.0 / 128.0) {
    EXPECT_NEAR(PortableErf(x), std::erf(x), 1e-15) << x;
  }
  EXPECT_EQ(PortableErf(7.0), 1.0);
  EXPECT_EQ(PortableErf(0.0), 0.0);
}

TEST(PortableFunctionsTest, AtanAndAcosMatchTheLibrary) {
  for (double x = -50.0; x <= 50.0; x += 1.0 / 64.0) {
    EXPECT_LE(UlpDistance(PortableAtan(x), std::atan(x)), 4.0) << x;
  }
  for (double y = -1.0; y <= 1.0; y += 1.0 / 1024.0) {
    EXPECT_NEAR(PortableAcos(y), std::acos(y), 1e-15) << y;
  }
  EXPECT_NEAR(PortableAcos(-1.0), std::numbers::pi, 1e-15);
  EXPECT_EQ(PortableAcos(1.0), 0.0);
}

TEST(PortableFunctionsTest, SmoothStepFallsFromOneToZeroSymmetrically) {
  EXPECT_EQ(SmoothStepDown(-0.1), 1.0);
  EXPECT_EQ(SmoothStepDown(1.1), 0.0);
  EXPECT_DOUBLE_EQ(SmoothStepDown(0.5), 0.5);
  for (double s = 0.01; s < 1.0; s += 0.01) {
    EXPECT_NEAR(SmoothStepDown(s) + SmoothStepDown(1.0 - s), 1.0, 1e-15);
  }
  for (double s = 0.1; s < 0.9; s += 0.01) {
    EXPECT_LT(SmoothStepDown(s + 0.005), SmoothStepDown(s));
  }
}

TEST(FastFourierTransformTest, MatchesTheDirectTransformAndInverts) {
  const std::size_t n = 64;
  std::vector<ComplexValue> signal(n);
  for (std::size_t i = 0; i < n; ++i) {
    signal[i] = {.re = std::sin(0.3 * static_cast<double>(i)) + 0.1,
                 .im = std::cos(0.17 * static_cast<double>(i * i))};
  }
  std::vector<ComplexValue> spectrum = signal;
  FastFourierTransform(&spectrum, /*inverse=*/false);
  for (std::size_t k = 0; k < n; ++k) {
    ComplexValue direct;
    for (std::size_t i = 0; i < n; ++i) {
      const double angle = -2.0 * std::numbers::pi *
                           static_cast<double>(k * i) / static_cast<double>(n);
      direct = direct + (signal[i] * ComplexValue{.re = std::cos(angle),
                                                  .im = std::sin(angle)});
    }
    EXPECT_NEAR(spectrum[k].re, direct.re, 1e-12);
    EXPECT_NEAR(spectrum[k].im, direct.im, 1e-12);
  }
  FastFourierTransform(&spectrum, /*inverse=*/true);
  for (std::size_t i = 0; i < n; ++i) {
    EXPECT_NEAR(spectrum[i].re / static_cast<double>(n), signal[i].re, 1e-14);
    EXPECT_NEAR(spectrum[i].im / static_cast<double>(n), signal[i].im, 1e-14);
  }
}

TEST(FastFourierTransformTest, RejectsALengthThatIsNotAPowerOfTwo) {
  std::vector<ComplexValue> values(12);
  EXPECT_THROW(FastFourierTransform(&values, false), std::invalid_argument);
}

TEST(GaussLegendreTest, IntegratesPolynomialsOfDegreeTwoNMinusOneExactly) {
  const GaussLegendreRule rule = MakeGaussLegendreRule(5);
  for (int degree = 0; degree <= 9; ++degree) {
    double sum = 0.0;
    for (std::size_t i = 0; i < rule.nodes.size(); ++i) {
      sum += rule.weights[i] * std::pow(rule.nodes[i], degree);
    }
    const double exact = degree % 2 == 1 ? 0.0 : 2.0 / (degree + 1.0);
    EXPECT_NEAR(sum, exact, 1e-15) << degree;
  }
}

// A Gaussian kernel, truncated where it is far below the test tolerance.
double GaussianKernel(double u) {
  return std::exp(-0.5 * u * u) / std::sqrt(2.0 * std::numbers::pi);
}

TEST(EdgeResponseTableTest, ReproducesTheIntegralOfItsKernel) {
  const EdgeResponseTable table(GaussianKernel, 12.0, 1.0 / 64.0);
  EXPECT_EQ(table(-12.0), 0.0);
  EXPECT_NEAR(table.Mass(), 1.0, 1e-14);
  EXPECT_EQ(table(13.0), table.Mass());
  for (double u = -6.0; u <= 6.0; u += 0.0137) {
    const double exact = 0.5 * (1.0 + std::erf(u / std::numbers::sqrt2));
    EXPECT_NEAR(table(u), exact, 1e-10) << u;
  }
}

TEST(EdgeResponseTableTest, TransferIsTheKernelsFourierTransform) {
  const EdgeResponseTable table(GaussianKernel, 12.0, 1.0 / 64.0);
  for (double f = 0.0; f <= 1.0; f += 0.05) {
    const double exact =
        std::exp(-2.0 * std::numbers::pi * std::numbers::pi * f * f);
    EXPECT_NEAR(table.Transfer(f), exact, 1e-12) << f;
  }
}

TEST(EdgeResponseTableTest, RejectsAWidthThatIsNotWholeSpacings) {
  EXPECT_THROW(EdgeResponseTable(GaussianKernel, 1.0, 0.3),
               std::invalid_argument);
}

}  // namespace
}  // namespace ld_creator
