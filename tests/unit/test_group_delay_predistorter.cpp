/*
 * File:        test_group_delay_predistorter.cpp
 * Module:      tests
 * Purpose:     Unit tests for S3, the group-delay pre-distorter
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>

#include "ld_creator/group_delay_predistorter/group_delay_predistorter.h"
#include "ld_creator/standards/ntsc_timing.h"
#include "ld_creator/video_rate_converter/video_rate_converter.h"
#include "sinusoid_fit.h"

namespace ld_creator {
namespace {

// IEC 60857-1986 9.1.7 tolerances, ns, in table order.
constexpr double kTolerancesNs[] = {0.0, 15.0, 15.0, 15.0, 30.0, 50.0};

double RelativeDelayNs(const GroupDelayFilter& filter, double frequency_hz) {
  return (RealisedGroupDelaySeconds(filter, frequency_hz) -
          RealisedGroupDelaySeconds(filter, 0.5e6)) *
         1e9;
}

std::vector<double> RunStage(const GroupDelayPredistorterConfig& config,
                             const std::vector<double>& input,
                             std::size_t block) {
  GroupDelayPredistorter stage(config);
  std::vector<double> output;
  IreSampleBlock out;
  for (std::size_t start = 0; start < input.size(); start += block) {
    IreSampleBlock in;
    in.first_sample = static_cast<std::int64_t>(start);
    const std::size_t end = std::min(input.size(), start + block);
    in.ire.assign(input.begin() + static_cast<std::ptrdiff_t>(start),
                  input.begin() + static_cast<std::ptrdiff_t>(end));
    stage.Process(in, &out);
    EXPECT_EQ(out.first_sample, static_cast<std::int64_t>(output.size()));
    output.insert(output.end(), out.ire.begin(), out.ire.end());
  }
  stage.Finish(&out);
  output.insert(output.end(), out.ire.begin(), out.ire.end());
  return output;
}

// Plan Task 3.2 acceptance.
TEST(GroupDelayPredistorterTest, DelayIsInTheMiddleThirdAtEveryTableFrequency) {
  const std::vector<GroupDelayPoint> table = DefaultGroupDelayTable();
  const GroupDelayFilter filter = DesignGroupDelayFilter(table);
  for (std::size_t i = 1; i < table.size(); ++i) {
    const double realised =
        RelativeDelayNs(filter, table[i].frequency_mhz * 1e6);
    EXPECT_NEAR(realised, table[i].delay_ns, kTolerancesNs[i] / 3.0)
        << table[i].frequency_mhz << " MHz";
  }
}

TEST(GroupDelayPredistorterTest, DelayStaysInsideTheFigureEightBand) {
  // Figure 8: ±15 ns below 0.5 MHz; above it, nominal and tolerance are
  // straight lines between the table points.
  const std::vector<GroupDelayPoint> table = DefaultGroupDelayTable();
  const GroupDelayFilter filter = DesignGroupDelayFilter(table);
  for (double f = 0.01e6; f <= 4.2e6; f += 0.01e6) {
    double nominal = 0.0;
    double tolerance = 15.0;
    for (std::size_t i = 1; i < table.size(); ++i) {
      const double f0 = table[i - 1].frequency_mhz * 1e6;
      const double f1 = table[i].frequency_mhz * 1e6;
      if (f >= f0 && f <= f1) {
        const double a = (f - f0) / (f1 - f0);
        nominal = table[i - 1].delay_ns +
                  (a * (table[i].delay_ns - table[i - 1].delay_ns));
        tolerance =
            (i == 1 ? 15.0 : kTolerancesNs[i - 1]) +
            (a * (kTolerancesNs[i] - (i == 1 ? 15.0 : kTolerancesNs[i - 1])));
      }
    }
    EXPECT_NEAR(RelativeDelayNs(filter, f), nominal, tolerance)
        << f / 1e6 << " MHz";
  }
}

TEST(GroupDelayPredistorterTest, MagnitudeIsFlatToFourPointTwoMegahertz) {
  const GroupDelayFilter filter =
      DesignGroupDelayFilter(DefaultGroupDelayTable());
  for (double f = 0.0; f <= 4.2e6; f += 0.01e6) {
    EXPECT_LT(std::abs(20.0 * std::log10(RealisedMagnitude(filter, f))), 0.1)
        << f / 1e6 << " MHz";
  }
  double sum = 0.0;
  for (const double tap : filter.taps) {
    sum += tap;
  }
  EXPECT_NEAR(sum, 1.0, 1e-15);
}

TEST(GroupDelayPredistorterTest, FollowsTheTargetCurveBetweenTablePoints) {
  const std::vector<GroupDelayPoint> table = DefaultGroupDelayTable();
  EXPECT_EQ(TargetGroupDelayNs(table, 0.2e6), 0.0);
  EXPECT_EQ(TargetGroupDelayNs(table, 3.58e6), -80.0);
  EXPECT_EQ(TargetGroupDelayNs(table, 5.0e6), -200.0);
  const double between = TargetGroupDelayNs(table, 3.8e6);
  EXPECT_LT(between, -80.0);
  EXPECT_GT(between, -135.0);
}

TEST(GroupDelayPredistorterTest, StreamedSignalHasTheDesignedPhaseResponse) {
  // A 3.58 MHz sinusoid through the running stage is shifted by exactly the
  // designed filter's phase, including the advance.
  constexpr double kFrequency = 3.58e6;
  constexpr double kAmplitude = 40.0;
  constexpr std::size_t kSamples = 60000;
  constexpr std::size_t kEdge = 2000;
  const double omega = 2.0 * std::numbers::pi * kFrequency;
  std::vector<double> input(kSamples);
  std::vector<double> times(kSamples);
  for (std::size_t m = 0; m < kSamples; ++m) {
    times[m] = static_cast<double>(m) / kModellingRateHz;
    input[m] = kAmplitude * std::cos(omega * times[m]);
  }
  const std::vector<double> output =
      RunStage(GroupDelayPredistorterConfig{}, input, 7777);
  ASSERT_EQ(output.size(), input.size());
  const test::SinusoidFit fit = test::FitSinusoid(
      std::span<const double>(output).subspan(kEdge, kSamples - (2 * kEdge)),
      std::span<const double>(times).subspan(kEdge, kSamples - (2 * kEdge)),
      kFrequency);

  const GroupDelayFilter filter =
      DesignGroupDelayFilter(DefaultGroupDelayTable());
  double re = 0.0;
  double im = 0.0;
  for (std::size_t k = 0; k < filter.taps.size(); ++k) {
    const double angle = omega * static_cast<double>(k) / ntsc::kSampleRateHz;
    re += filter.taps[k] * std::cos(angle);
    im -= filter.taps[k] * std::sin(angle);
  }
  const double designed_phase =
      std::atan2(im, re) +
      (omega * static_cast<double>(filter.advance_samples) / kModellingRateHz);
  const test::SinusoidFit designed{.amplitude = 0.0,
                                   .phase_rad = -designed_phase};
  const test::SinusoidFit reference{.amplitude = 0.0, .phase_rad = 0.0};

  EXPECT_NEAR(fit.amplitude, kAmplitude * std::hypot(re, im), 1e-9);
  EXPECT_NEAR(test::DelayBetween(fit, reference, kFrequency),
              test::DelayBetween(designed, reference, kFrequency), 1e-15);
}

TEST(GroupDelayPredistorterTest, PassesTheSignalUnchangedWhenDisabled) {
  GroupDelayPredistorterConfig config;
  config.enabled = false;
  const std::vector<double> input = {0.0, 1.0, -40.0, 100.0, 7.5};
  EXPECT_EQ(RunStage(config, input, 2), input);
}

TEST(GroupDelayPredistorterTest, KeepsConstantLevelsAndIsBlockSizeIndependent) {
  std::vector<double> input(3000, 100.0);
  for (std::size_t m = 0; m < 1500; ++m) {
    input[m] = std::sin(0.05 * static_cast<double>(m)) * 50.0;
  }
  const std::vector<double> reference =
      RunStage(GroupDelayPredistorterConfig{}, input, input.size());
  for (const std::size_t block : {1U, 6U, 999U}) {
    EXPECT_EQ(RunStage(GroupDelayPredistorterConfig{}, input, block),
              reference);
  }
  // Well inside the constant tail, the output is 100 IRE to rounding.
  EXPECT_NEAR(reference[2400], 100.0, 1e-12);
}

}  // namespace
}  // namespace ld_creator
