/*
 * File:        test_video_pre_emphasis.cpp
 * Module:      tests
 * Purpose:     Unit tests for S4, the video pre-emphasis
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

#include "ld_creator/video_pre_emphasis/video_pre_emphasis.h"
#include "ld_creator/video_rate_converter/video_rate_converter.h"
#include "sinusoid_fit.h"

namespace ld_creator {
namespace {

std::vector<double> RunStage(const std::vector<double>& input,
                             std::size_t block) {
  VideoPreEmphasis stage{VideoPreEmphasisConfig{}};
  std::vector<double> output;
  IreSampleBlock out;
  for (std::size_t start = 0; start < input.size(); start += block) {
    IreSampleBlock in;
    in.first_sample = static_cast<std::int64_t>(start);
    const std::size_t end = std::min(input.size(), start + block);
    in.ire.assign(input.begin() + static_cast<std::ptrdiff_t>(start),
                  input.begin() + static_cast<std::ptrdiff_t>(end));
    stage.Process(in, &out);
    EXPECT_EQ(out.first_sample, in.first_sample);
    output.insert(output.end(), out.ire.begin(), out.ire.end());
  }
  stage.Finish(&out);
  EXPECT_TRUE(out.ire.empty());
  return output;
}

// Plan Task 3.3 acceptance: within 0.05 dB of |H(jω)| from 0.1 to 6 MHz.
TEST(VideoPreEmphasisTest, MagnitudeMatchesTheAnalogueShelf) {
  const VideoPreEmphasisConfig config;
  const PreEmphasisCoefficients coefficients = DesignPreEmphasis(config);
  for (double f = 0.1e6; f <= 6.0e6; f += 0.01e6) {
    const double error_db =
        20.0 * std::log10(DiscretePreEmphasisMagnitude(coefficients, f) /
                          AnaloguePreEmphasisMagnitude(config, f));
    EXPECT_LT(std::abs(error_db), 0.05) << f / 1e6 << " MHz";
  }
}

TEST(VideoPreEmphasisTest, AnalogueShelfHasTheDerivedValues) {
  // S4 note: +8.04 dB at fsc, t₁/t₂ = 2.667 at high frequency.
  const VideoPreEmphasisConfig config;
  EXPECT_NEAR(
      20.0 * std::log10(AnaloguePreEmphasisMagnitude(config, 3.579545e6)), 8.04,
      0.005);
  EXPECT_NEAR(AnaloguePreEmphasisMagnitude(config, 1e12), 320.0 / 120.0, 1e-6);
}

TEST(VideoPreEmphasisTest, PassesAConstantLevelExactly) {
  const std::vector<double> output =
      RunStage(std::vector<double>(4000, 100.0), 512);
  EXPECT_EQ(output.back(), 100.0);
  EXPECT_EQ(DiscretePreEmphasisMagnitude(
                DesignPreEmphasis(VideoPreEmphasisConfig{}), 0.0),
            1.0);
}

TEST(VideoPreEmphasisTest, StepOvershootsByTheTimeConstantRatio) {
  // A step of Δ first appears as Δ·(1 + shelf·gain), which tends to Δ·t₁/t₂
  // as Fm → ∞ (S4 note, "Overshoot"); at Fm it is within 3 %.
  std::vector<double> input(400, 0.0);
  std::fill(input.begin() + 100, input.end(), 140.0);
  const std::vector<double> output = RunStage(input, 64);
  EXPECT_NEAR(output[100] / 140.0, 320.0 / 120.0, 0.03 * 320.0 / 120.0);
  EXPECT_NEAR(output.back(), 140.0, 1e-9);
}

TEST(VideoPreEmphasisTest, StreamedSubcarrierHasTheDesignedGain) {
  constexpr double kFrequency = 3.579545e6;
  std::vector<double> input(30000);
  std::vector<double> times(input.size());
  for (std::size_t m = 0; m < input.size(); ++m) {
    times[m] = static_cast<double>(m) / kModellingRateHz;
    input[m] = 20.0 * std::cos(2.0 * std::numbers::pi * kFrequency * times[m]);
  }
  const std::vector<double> output = RunStage(input, 1000);
  const test::SinusoidFit fit = test::FitSinusoid(
      std::span<const double>(output).subspan(5000),
      std::span<const double>(times).subspan(5000), kFrequency);
  EXPECT_NEAR(fit.amplitude / 20.0,
              DiscretePreEmphasisMagnitude(
                  DesignPreEmphasis(VideoPreEmphasisConfig{}), kFrequency),
              1e-9);
}

TEST(VideoPreEmphasisTest, OutputDoesNotDependOnBlockSize) {
  std::vector<double> input(2000);
  for (std::size_t m = 0; m < input.size(); ++m) {
    input[m] = std::sin(0.11 * static_cast<double>(m)) * 70.0;
  }
  const std::vector<double> reference = RunStage(input, input.size());
  EXPECT_EQ(RunStage(input, 1), reference);
  EXPECT_EQ(RunStage(input, 333), reference);
}

}  // namespace
}  // namespace ld_creator
