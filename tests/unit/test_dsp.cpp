/*
 * File:        test_dsp.cpp
 * Module:      tests
 * Purpose:     Unit tests for the shared DSP helpers
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

#include "ld_creator/dsp/hermite_phase.h"
#include "ld_creator/dsp/kaiser_window.h"
#include "ld_creator/dsp/linear_solver.h"
#include "ld_creator/dsp/monotone_cubic.h"
#include "ld_creator/dsp/portable_math.h"

namespace ld_creator {
namespace {

TEST(PortableMathTest, GivesExactValuesAtQuarterTurns) {
  EXPECT_EQ(CosTurns(0.0), 1.0);
  EXPECT_EQ(SinTurns(0.0), 0.0);
  EXPECT_EQ(CosTurns(0.5), -1.0);
  EXPECT_EQ(SinTurns(0.25), 1.0);
  EXPECT_EQ(SinTurns(-0.25), -1.0);
  EXPECT_EQ(CosTurns(1.0e9), 1.0);
}

TEST(PortableMathTest, MatchesTheLibraryWithinOneTurn) {
  // Within ±½ turn the reference angle 2π·turns is itself rounded by at most
  // ½ ulp(π) = 2.2e-16, so the comparison is limited to about 4e-16.
  for (int i = -4000; i <= 4000; ++i) {
    const double turns = i * 0.000123;
    const double angle = 2.0 * std::numbers::pi * turns;
    EXPECT_NEAR(SinTurns(turns), std::sin(angle), 6e-16) << turns;
    EXPECT_NEAR(CosTurns(turns), std::cos(angle), 6e-16) << turns;
  }
}

TEST(PortableMathTest, IsExactlyPeriodicInWholeTurns) {
  for (int i = -100; i <= 100; ++i) {
    const double turns = i * 0.00390625;  // exact in binary
    EXPECT_EQ(SinTurns(turns + 1000.0), SinTurns(turns)) << turns;
    EXPECT_EQ(CosTurns(turns - 7.0), CosTurns(turns)) << turns;
  }
}

TEST(PortableMathTest, ReducesLargeArgumentsByWholeTurns) {
  const double fraction = 0.123;
  const SinCosValue near = SinCosTurns(fraction);
  const SinCosValue far = SinCosTurns(4.0e10 + fraction);
  // 4e10 + 0.123 is represented to 2^-17 turns.
  EXPECT_NEAR(far.sin, near.sin, 1e-4);
  EXPECT_NEAR(far.cos, near.cos, 1e-4);
}

TEST(KaiserWindowTest, MatchesBesselValuesAndWindowShape) {
  EXPECT_EQ(BesselI0(0.0), 1.0);
  // Abramowitz & Stegun Table 9.8: I0(1) = 1.266065878, I0(5) = 27.23987182.
  EXPECT_NEAR(BesselI0(1.0), 1.2660658777520082, 1e-15);
  EXPECT_NEAR(BesselI0(5.0), 27.239871823604442, 1e-12);
  EXPECT_EQ(KaiserWindow(0.0, 9.0), 1.0);
  EXPECT_NEAR(KaiserWindow(1.0, 9.0), 1.0 / BesselI0(9.0), 1e-18);
  EXPECT_EQ(KaiserWindow(1.5, 9.0), 0.0);
  EXPECT_EQ(KaiserWindow(-0.3, 9.0), KaiserWindow(0.3, 9.0));
}

TEST(LinearSolverTest, SolvesASymmetricPositiveDefiniteSystem) {
  SquareMatrix matrix(3);
  const double values[3][3] = {{4, 1, 2}, {1, 5, 3}, {2, 3, 6}};
  for (std::size_t r = 0; r < 3; ++r) {
    for (std::size_t c = 0; c < 3; ++c) {
      matrix.At(r, c) = values[r][c];
    }
  }
  // x = (1, −2, 3): b = A·x.
  const std::vector<double> x =
      SolveSymmetricPositiveDefinite(matrix, {8.0, 0.0, 14.0});
  EXPECT_NEAR(x[0], 1.0, 1e-14);
  EXPECT_NEAR(x[1], -2.0, 1e-14);
  EXPECT_NEAR(x[2], 3.0, 1e-14);
}

TEST(LinearSolverTest, RejectsAMatrixThatIsNotPositiveDefinite) {
  SquareMatrix matrix(2);
  matrix.At(0, 0) = 1.0;
  matrix.At(0, 1) = 2.0;
  matrix.At(1, 0) = 2.0;
  matrix.At(1, 1) = 1.0;
  EXPECT_THROW(SolveSymmetricPositiveDefinite(matrix, {1.0, 1.0}),
               std::runtime_error);
}

TEST(MonotoneCubicTest, PassesThroughEveryPointAndHoldsOutside) {
  const MonotoneCubic curve({0.0, 0.5, 2.0, 3.0}, {0.0, 0.0, -15.0, -45.0});
  EXPECT_EQ(curve.Value(0.5), 0.0);
  EXPECT_EQ(curve.Value(2.0), -15.0);
  EXPECT_EQ(curve.Value(3.0), -45.0);
  EXPECT_EQ(curve.Value(-1.0), 0.0);
  EXPECT_EQ(curve.Value(10.0), -45.0);
  EXPECT_EQ(curve.Value(0.25), 0.0);
}

TEST(MonotoneCubicTest, StaysBetweenNeighbouringValues) {
  const MonotoneCubic curve({0.0, 0.5, 2.0, 3.0, 3.58, 4.0, 4.2},
                            {0.0, 0.0, -15.0, -45.0, -80.0, -135.0, -200.0});
  double previous = curve.Value(0.0);
  for (double x = 0.0; x <= 4.2; x += 0.001) {
    const double value = curve.Value(x);
    EXPECT_LE(value, previous + 1e-12) << x;
    previous = value;
  }
}

TEST(MonotoneCubicTest, IntegratesExactlyAndContinuesBeyondTheEnds) {
  // The end slopes are 0, so the curve bends near the ends, but it is
  // symmetric about (1, 1) and so its integral over [0, 2] is still 2.
  const MonotoneCubic line({0.0, 1.0, 2.0}, {0.0, 1.0, 2.0});
  EXPECT_NEAR(line.Integral(2.0), 2.0, 1e-15);
  EXPECT_NEAR(line.Integral(3.0), 4.0, 1e-15);
  EXPECT_NEAR(line.Integral(-1.0), 0.0, 1e-15);
  const MonotoneCubic flat({0.0, 1.0}, {3.0, 3.0});
  EXPECT_NEAR(flat.Integral(0.4), 1.2, 1e-15);
}

TEST(HermitePhaseTest, IsExactForAConstantFrequency) {
  const double f = 8.1e6;
  const double h = 1.0 / 85.0e6;
  const double step = 2.0 * std::numbers::pi * f * h;
  for (double u = 0.0; u <= 1.0; u += 0.125) {
    EXPECT_NEAR(HermitePhaseIncrement(step, f, f, h, u), step * u, 1e-15);
  }
}

TEST(HermitePhaseTest, IsExactForALinearFrequencySweep) {
  // f(t) = f0 + r·t, φ(t) = 2π(f0·t + r·t²/2): a quadratic, which a cubic
  // Hermite interpolant reproduces.
  const double f0 = 8.0e6;
  const double rate = 1.0e13;
  const double h = 1.0 / 85.0e6;
  const auto phase = [&](double t) {
    return 2.0 * std::numbers::pi * ((f0 * t) + (0.5 * rate * t * t));
  };
  for (double u = 0.0; u <= 1.0; u += 0.125) {
    EXPECT_NEAR(HermitePhaseIncrement(phase(h), f0, f0 + (rate * h), h, u),
                phase(u * h), 1e-14);
  }
}

}  // namespace
}  // namespace ld_creator
