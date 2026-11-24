/*
 * File:        test_video_rate_converter.cpp
 * Module:      tests
 * Purpose:     Unit tests for S2, the video rate converter
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
#include <vector>

#include "ld_creator/standards/ntsc_timing.h"
#include "ld_creator/video_rate_converter/video_rate_converter.h"
#include "sinusoid_fit.h"

namespace ld_creator {
namespace {

// S2 note, requirement 1: ≤ 0.01 dB and ≤ 0.1 ns from 0 to 6 MHz.
constexpr double kPassbandDb = 0.01;
constexpr double kPassbandDelayS = 0.1e-9;

double InputTime(std::int64_t n) {
  return (static_cast<double>(n) + ntsc::kStoredOriginSamples) /
         ntsc::kSampleRateHz;
}

// Converts `input` in blocks of `block` samples.
std::vector<double> Convert(const std::vector<double>& input,
                            std::size_t block) {
  VideoRateConverter converter;
  std::vector<double> output;
  IreSampleBlock out;
  for (std::size_t start = 0; start < input.size(); start += block) {
    IreSampleBlock in;
    in.first_sample = static_cast<std::int64_t>(start);
    const std::size_t end = std::min(input.size(), start + block);
    in.ire.assign(input.begin() + static_cast<std::ptrdiff_t>(start),
                  input.begin() + static_cast<std::ptrdiff_t>(end));
    converter.Process(in, &out);
    EXPECT_EQ(out.first_sample, static_cast<std::int64_t>(output.size()));
    output.insert(output.end(), out.ire.begin(), out.ire.end());
  }
  converter.Finish(&out);
  EXPECT_EQ(out.first_sample, static_cast<std::int64_t>(output.size()));
  output.insert(output.end(), out.ire.begin(), out.ire.end());
  return output;
}

class RateConverterPassbandTest : public ::testing::TestWithParam<double> {};

// Plan Task 3.1 acceptance: a 4fsc sinusoid converted to Fm matches the
// analytic signal within the passband bound.
TEST_P(RateConverterPassbandTest,
       MatchesTheAnalyticSinusoidWithinThePassbandBound) {
  const double frequency = GetParam();
  constexpr double kAmplitude = 50.0;
  constexpr std::int64_t kInputSamples = 20000;
  std::vector<double> input(kInputSamples);
  for (std::int64_t n = 0; n < kInputSamples; ++n) {
    input[static_cast<std::size_t>(n)] =
        kAmplitude *
        std::cos(2.0 * std::numbers::pi * frequency * InputTime(n));
  }
  const std::vector<double> output = Convert(input, 4096);
  ASSERT_EQ(output.size(), static_cast<std::size_t>(6 * kInputSamples));

  // Fit away from the ends, where the run's zero padding is in the window.
  std::vector<double> values;
  std::vector<double> times;
  for (std::size_t m = 6000; m + 6000 < output.size(); ++m) {
    values.push_back(output[m]);
    times.push_back(static_cast<double>(m) / kModellingRateHz);
  }
  const test::SinusoidFit fit = test::FitSinusoid(values, times, frequency);
  const double gain_db = 20.0 * std::log10(fit.amplitude / kAmplitude);
  const double delay = fit.phase_rad / (2.0 * std::numbers::pi * frequency);
  EXPECT_LT(std::abs(gain_db), kPassbandDb);
  EXPECT_LT(std::abs(delay), kPassbandDelayS);

  // Nothing but the sinusoid: images and kernel error are below −80 dB.
  double worst = 0.0;
  for (std::size_t i = 0; i < values.size(); ++i) {
    const double expected =
        fit.amplitude *
        std::cos((2.0 * std::numbers::pi * frequency * times[i]) -
                 fit.phase_rad);
    worst = std::max(worst, std::abs(values[i] - expected));
  }
  EXPECT_LT(worst, kAmplitude * 1e-4);
}

INSTANTIATE_TEST_SUITE_P(TableFrequencies, RateConverterPassbandTest,
                         ::testing::Values(0.5e6, 3.579545e6, 4.2e6));

TEST(VideoRateConverterTest, RunsAtTwentyFourTimesTheSubcarrier) {
  EXPECT_DOUBLE_EQ(kModellingRateHz, 24.0 * ntsc::kSubcarrierFrequencyHz);
  EXPECT_NEAR(kModellingRateHz, 945.0e6 / 11.0, 1e-6);
  EXPECT_EQ(kModellingSamplesPerFrame, 2866500);
}

TEST(VideoRateConverterTest, PhasesHaveUnitDcGain) {
  for (const InterpolatorPhase& phase : InterpolatorPhases()) {
    double sum = 0.0;
    for (const double weight : phase) {
      sum += weight;
    }
    EXPECT_NEAR(sum, 1.0, 1e-15);
  }
}

TEST(VideoRateConverterTest, ReproducesAConstantLevelAwayFromTheEnds) {
  const std::vector<double> output =
      Convert(std::vector<double>(2000, 7.5), 300);
  for (std::size_t m = 200; m + 200 < output.size(); ++m) {
    EXPECT_NEAR(output[m], 7.5, 1e-13) << m;
  }
}

TEST(VideoRateConverterTest, OutputDoesNotDependOnBlockSize) {
  std::vector<double> input(5000);
  for (std::size_t n = 0; n < input.size(); ++n) {
    input[n] = std::sin(0.37 * static_cast<double>(n)) * 60.0;
  }
  const std::vector<double> reference = Convert(input, input.size());
  for (const std::size_t block : {1U, 7U, 910U, 4999U}) {
    EXPECT_EQ(Convert(input, block), reference) << block;
  }
}

}  // namespace
}  // namespace ld_creator
