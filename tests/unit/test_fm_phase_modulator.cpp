/*
 * File:        test_fm_phase_modulator.cpp
 * Module:      tests
 * Purpose:     Unit tests for S5, the FM phase modulator
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

#include "ld_creator/fm_phase_modulator/fm_phase_modulator.h"
#include "ld_creator/video_rate_converter/video_rate_converter.h"

namespace ld_creator {
namespace {

CarrierPhaseBlock RunStage(const FmPhaseModulatorConfig& config,
                           const std::vector<double>& input,
                           std::size_t block) {
  FmPhaseModulator stage(config);
  CarrierPhaseBlock all;
  CarrierPhaseBlock out;
  const auto append = [&all](const CarrierPhaseBlock& part) {
    EXPECT_EQ(part.first_sample,
              static_cast<std::int64_t>(all.phase_rad.size()));
    EXPECT_EQ(part.frequency_hz.size(), part.phase_rad.size());
    EXPECT_EQ(part.slice_level.size(), part.phase_rad.size());
    all.phase_rad.insert(all.phase_rad.end(), part.phase_rad.begin(),
                         part.phase_rad.end());
    all.frequency_hz.insert(all.frequency_hz.end(), part.frequency_hz.begin(),
                            part.frequency_hz.end());
    all.slice_level.insert(all.slice_level.end(), part.slice_level.begin(),
                           part.slice_level.end());
  };
  for (std::size_t start = 0; start < input.size(); start += block) {
    IreSampleBlock in;
    in.first_sample = static_cast<std::int64_t>(start);
    const std::size_t end = std::min(input.size(), start + block);
    in.ire.assign(input.begin() + static_cast<std::ptrdiff_t>(start),
                  input.begin() + static_cast<std::ptrdiff_t>(end));
    stage.Process(in, &out);
    append(out);
  }
  stage.Finish(&out);
  append(out);
  return all;
}

// Carrier frequency from the slope of φ over the samples away from the ends.
double SlopeFrequency(const CarrierPhaseBlock& block, std::size_t from,
                      std::size_t to) {
  return (block.phase_rad[to] - block.phase_rad[from]) /
         (2.0 * std::numbers::pi * static_cast<double>(to - from) /
          kModellingRateHz);
}

struct LevelCase {
  double ire;
  double expected_hz;
};

class FmFrequencyLawTest : public ::testing::TestWithParam<LevelCase> {};

// Plan Task 3.4 acceptance: −40, 0 and 100 IRE give 7.614286, 8.1 and
// 9.314286 MHz from the slope of φ, to < 1 Hz.
TEST_P(FmFrequencyLawTest, ConstantLevelGivesTheIecCarrierFrequency) {
  const LevelCase level = GetParam();
  const CarrierPhaseBlock phase = RunStage(
      FmPhaseModulatorConfig{}, std::vector<double>(200000, level.ire), 4096);
  ASSERT_EQ(phase.phase_rad.size(), 200001U);
  EXPECT_NEAR(SlopeFrequency(phase, 10, 199990), level.expected_hz, 1.0);
  EXPECT_NEAR(phase.frequency_hz[1000], level.expected_hz, 1e-6);
}

INSTANTIATE_TEST_SUITE_P(
    IecLevels, FmFrequencyLawTest,
    ::testing::Values(LevelCase{-40.0, 8.1e6 - (40.0 * 1.7e6 / 140.0)},
                      LevelCase{0.0, 8.1e6},
                      LevelCase{100.0, 8.1e6 + (100.0 * 1.7e6 / 140.0)}));

TEST(FmPhaseModulatorTest, NamedFrequenciesMatchThePlan) {
  EXPECT_NEAR(8.1e6 - (40.0 * 1.7e6 / 140.0), 7.614286e6, 0.5);
  EXPECT_NEAR(8.1e6 + (100.0 * 1.7e6 / 140.0), 9.314286e6, 0.5);
}

TEST(FmPhaseModulatorTest, StartsAtTheInitialPhaseWithZeroSliceLevel) {
  FmPhaseModulatorConfig config;
  config.initial_phase_rad = 1.25;
  const CarrierPhaseBlock phase =
      RunStage(config, std::vector<double>(100, 0.0), 100);
  EXPECT_EQ(phase.phase_rad.front(), 1.25);
  EXPECT_TRUE(std::all_of(phase.slice_level.begin(), phase.slice_level.end(),
                          [](double s) { return s == 0.0; }));
  // At blanking, one Fm interval advances 8.1 MHz / Fm turns.
  EXPECT_NEAR(phase.phase_rad[1] - phase.phase_rad[0],
              2.0 * std::numbers::pi * 8.1e6 / kModellingRateHz, 1e-15);
}

TEST(FmPhaseModulatorTest, IntegratesASinusoidalModulationAccurately) {
  // v = A sin ωt gives φ(t) = φ₀ + 2π(f_b·t + kA(1 − cos ωt)/ω); compare the
  // modulation part in the middle of the run.
  constexpr double kAmplitude = 50.0;
  constexpr double kVideoHz = 4.2e6;
  const double omega = 2.0 * std::numbers::pi * kVideoHz;
  const double hz_per_ire = 1.7e6 / 140.0;
  std::vector<double> input(40000);
  for (std::size_t m = 0; m < input.size(); ++m) {
    input[m] = kAmplitude *
               std::sin(omega * static_cast<double>(m) / kModellingRateHz);
  }
  const CarrierPhaseBlock phase =
      RunStage(FmPhaseModulatorConfig{}, input, 5000);
  const auto expected = [&](std::size_t m) {
    const double t = static_cast<double>(m) / kModellingRateHz;
    return 2.0 * std::numbers::pi *
           ((8.1e6 * t) +
            (hz_per_ire * kAmplitude * (1.0 - std::cos(omega * t)) / omega));
  };
  // Measured from sample 100, so the slope change where the run starts
  // (the input is 0 before it) does not enter.
  constexpr std::size_t kFrom = 100;
  double worst = 0.0;
  for (std::size_t m = kFrom; m < 39900; ++m) {
    worst =
        std::max(worst, std::abs((phase.phase_rad[m] - phase.phase_rad[kFrom]) -
                                 (expected(m) - expected(kFrom))));
  }
  // S5 note: the rule's response error at 4.2 MHz is 2.6e-6 of the
  // modulation's phase amplitude 2πkA/ω = 0.145 rad, so 4e-7 rad, plus the
  // rounding of φ itself (≈ 1e-11 rad here).
  EXPECT_LT(worst, 1e-6);
}

TEST(FmPhaseModulatorTest, OutputDoesNotDependOnBlockSize) {
  std::vector<double> input(5000);
  for (std::size_t m = 0; m < input.size(); ++m) {
    input[m] = std::sin(0.02 * static_cast<double>(m)) * 120.0;
  }
  const CarrierPhaseBlock reference =
      RunStage(FmPhaseModulatorConfig{}, input, input.size());
  for (const std::size_t block : {1U, 2U, 3U, 4999U}) {
    const CarrierPhaseBlock other =
        RunStage(FmPhaseModulatorConfig{}, input, block);
    EXPECT_EQ(other.phase_rad, reference.phase_rad) << block;
    EXPECT_EQ(other.frequency_hz, reference.frequency_hz) << block;
  }
}

}  // namespace
}  // namespace ld_creator
