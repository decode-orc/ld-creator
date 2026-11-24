/*
 * File:        test_player_front_end.cpp
 * Module:      tests
 * Purpose:     Unit tests for S12, the player front end and its equaliser
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "ld_creator/optical_readout/optical_readout.h"
#include "ld_creator/optical_readout/readout_kernel.h"
#include "ld_creator/optical_readout/readout_transfer.h"
#include "ld_creator/player_front_end/player_front_end.h"
#include "pit_train.h"
#include "sinusoid_fit.h"

namespace ld_creator {
namespace {

constexpr double kNa = 0.40;
constexpr double kRim = 0.23;
constexpr double kSpeed = 10.3577;

double Cutoff() { return 2.0 * kNa / kReadoutWavelengthM; }

const EdgeResponseTable& Equaliser() {
  static const EdgeResponseTable table = MakeBandLimitedEdgeResponse(Cutoff());
  return table;
}

TEST(ReferenceFrontEndTest, PassesTheTestPointRfUnchanged) {
  ReferenceFrontEnd front_end;
  ReadoutBlock input;
  input.photocurrent = {.first_sample = 17, .level = {1.0, 0.7, 0.25, 0.9}};
  PhotocurrentBlock output;
  front_end.Process(input, &output);
  EXPECT_EQ(output.first_sample, 17);
  EXPECT_EQ(output.level, input.photocurrent.level);
  front_end.Finish(&output);
  EXPECT_TRUE(output.level.empty());
}

// S12 note: the cascade is flat to 1e-6 wherever H > 0.05 and passes
// nothing beyond the optical cut-off.
TEST(BandLimitedEdgeTest, IsFlatBelowTheBandAndZeroAboveTheCutoff) {
  for (double nu = 0.0; nu <= 0.843 * Cutoff(); nu += 5e3) {
    EXPECT_NEAR(Equaliser().Transfer(nu), 1.0, 1e-6) << nu;
  }
  for (double nu = Cutoff(); nu < 2.0 * Cutoff(); nu += 5e3) {
    EXPECT_NEAR(Equaliser().Transfer(nu), 0.0, 1e-6) << nu;
  }
  // H = 0.05 is reached below the flat band.
  EXPECT_LT(ReadoutTransfer(0.843 * Cutoff(), kNa, kRim, kReadoutWavelengthM),
            0.05);
}

TEST(BandLimitedEdgeTest, OvershootsLikeTheBandLimitedStep) {
  // E(x) = ½ + Si(2π·fe·x)/π peaks at 1.0895 (Gibbs).
  double peak = 0.0;
  for (double u = 0.0; u < 5e-6; u += 1e-9) {
    peak = std::max(peak, Equaliser()(u));
  }
  EXPECT_NEAR(peak, 1.0895, 1e-3);
}

struct Rendered {
  std::vector<double> d8;
  std::vector<double> d9;
};

Rendered RenderBoth(double period_m, double length_m) {
  const test::ConstantSpeedTrajectory spot(kSpeed);
  const ReadoutExtent extent{
      .sample_count = static_cast<std::int64_t>(
          std::floor(length_m / kSpeed * kReadoutRateHz)),
      .end_arc_length_m = length_m};
  LinearOpticalReadout readout({.numerical_aperture = kNa,
                                .rim_intensity = kRim,
                                .hfmi = MaximumHfmi(kNa, kRim)},
                               &spot, extent);
  InverseMtfEqualiser equaliser(kNa, readout.Contrast(), &spot, extent);
  Rendered result;
  ReadoutBlock d8;
  PhotocurrentBlock d9;
  auto take = [&] {
    result.d8.insert(result.d8.end(), d8.photocurrent.level.begin(),
                     d8.photocurrent.level.end());
    equaliser.Process(d8, &d9);
    result.d9.insert(result.d9.end(), d9.level.begin(), d9.level.end());
  };
  for (const ScanBlock& block :
       test::PitTrain(period_m, length_m, kSpeed, 500)) {
    readout.Process(block, &d8);
    take();
  }
  readout.Finish(&d8);
  take();
  equaliser.Finish(&d9);
  result.d9.insert(result.d9.end(), d9.level.begin(), d9.level.end());
  return result;
}

double FundamentalAmplitude(const std::vector<double>& levels,
                            double period_m) {
  const double frequency = kSpeed / period_m;
  const double per_period = kReadoutRateHz / frequency;
  const auto skip = static_cast<std::size_t>(100e-6 / kSpeed * kReadoutRateHz);
  const auto count = static_cast<std::size_t>(std::round(
      std::floor(static_cast<double>(levels.size() - (2 * skip)) / per_period) *
      per_period));
  double mean = 0.0;
  for (std::size_t n = skip; n < skip + count; ++n) {
    mean += levels[n];
  }
  mean /= static_cast<double>(count);
  std::vector<double> values;
  std::vector<double> times;
  for (std::size_t n = skip; n < skip + count; ++n) {
    values.push_back(levels[n] - mean);
    times.push_back(static_cast<double>(n) / kReadoutRateHz);
  }
  return test::FitSinusoid(values, times, frequency).amplitude;
}

// Plan Task 5.3 / S12 note verification: with the inverse equaliser the D9
// fundamental is the pit pattern's own, (4/π)·m/2 peak, and D9/D8 = 1/K
// with K the S11 kernel's transfer.
TEST(InverseMtfEqualiserTest, RestoresThePitPatternsFundamental) {
  const EdgeResponseTable s11 = MakeReadoutEdgeResponse(kNa, kRim);
  // Periods of 26, 10 and 8 readout samples (297, 772 and 966 pits/mm), so
  // the fit spans whole periods exactly.
  for (int samples : {26, 10, 8}) {
    const double period = kSpeed * samples / kReadoutRateHz;
    const double pits_per_mm = 1e-3 / period;
    const Rendered rendered = RenderBoth(period, 0.6e-3);
    ASSERT_EQ(rendered.d8.size(), rendered.d9.size());
    const double d9 = FundamentalAmplitude(rendered.d9, period);
    const double d8 = FundamentalAmplitude(rendered.d8, period);
    EXPECT_NEAR(d9, 2.0 / std::numbers::pi, 2e-6) << pits_per_mm;
    EXPECT_NEAR(d9 / d8, 1.0 / s11.Transfer(1.0 / period),
                2e-6 / s11.Transfer(1.0 / period))
        << pits_per_mm;
  }
}

}  // namespace
}  // namespace ld_creator
