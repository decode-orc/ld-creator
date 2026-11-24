/*
 * File:        test_disc_cutter.cpp
 * Module:      tests
 * Purpose:     Unit tests for S8, the disc cutter and cutting trajectory
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

#include "ld_creator/disc_cutter/cutting_trajectory.h"
#include "ld_creator/disc_cutter/disc_cutter.h"
#include "ld_creator/recording_limiter/recording_limiter.h"
#include "ld_creator/standards/ntsc_timing.h"
#include "ld_creator/video_rate_converter/video_rate_converter.h"

namespace ld_creator {
namespace {

constexpr double kStartRadiusM = 55e-3;
constexpr double kPitchM = 1.6667e-6;
constexpr double kVelocityMS = 11.0;

DiscCutterConfig CavConfig() {
  return DiscCutterConfig{.format = DiscFormat::kCav,
                          .start_radius_mm = 55.0,
                          .track_pitch_um = 1.6667,
                          .linear_velocity_m_s = std::nullopt};
}

DiscCutterConfig ClvConfig() {
  return DiscCutterConfig{.format = DiscFormat::kClv,
                          .start_radius_mm = 55.0,
                          .track_pitch_um = 1.6667,
                          .linear_velocity_m_s = kVelocityMS};
}

// Plan Task 4.3 acceptance: CAV turns once per frame (IEC 60857 §4.7.1), so
// after N revolutions the radius has grown by N track pitches.
TEST(CuttingTrajectoryTest, CavRadiusAfterNRevolutionsAddsNPitches) {
  const CuttingTrajectory trajectory(CavConfig());
  EXPECT_DOUBLE_EQ(trajectory.RadiusAt(0.0), kStartRadiusM);
  for (const double revolutions : {1.0, 100.0, 54000.0}) {
    EXPECT_NEAR(trajectory.RadiusAt(revolutions / ntsc::kFrameRateHz),
                kStartRadiusM + (revolutions * kPitchM), 1e-15)
        << revolutions;
  }
}

// One CAV revolution is a circle at its mean radius: the arc length of
// revolution N is 2π(r₀ + (N − ½)p).
TEST(CuttingTrajectoryTest, CavArcLengthPerRevolutionIsTheMeanCircumference) {
  const CuttingTrajectory trajectory(CavConfig());
  EXPECT_DOUBLE_EQ(trajectory.ArcLengthAt(0.0), 0.0);
  for (const double revolution : {1.0, 1000.0, 54000.0}) {
    const double length =
        trajectory.ArcLengthAt(revolution / ntsc::kFrameRateHz) -
        trajectory.ArcLengthAt((revolution - 1.0) / ntsc::kFrameRateHz);
    EXPECT_NEAR(length,
                2.0 * std::numbers::pi *
                    (kStartRadiusM + ((revolution - 0.5) * kPitchM)),
                1e-9)
        << revolution;
  }
}

// Plan Task 4.3 acceptance for CLV: the revolutions turned, counted
// independently as ∫ v/(2πr) dt, account for the radius gained at one
// pitch per revolution, and the arc length is v·t (§4.7.2).
TEST(CuttingTrajectoryTest, ClvRadiusGrowsOnePitchPerRevolution) {
  const CuttingTrajectory trajectory(ClvConfig());
  constexpr double kDurationS = 1800.0;
  constexpr int kSteps = 200000;  // even, for Simpson's rule
  const double h = kDurationS / kSteps;
  double sum = 0.0;
  for (int i = 0; i <= kSteps; ++i) {
    const double weight =
        (i == 0 || i == kSteps) ? 1.0 : ((i % 2 == 1) ? 4.0 : 2.0);
    sum += weight * kVelocityMS /
           (2.0 * std::numbers::pi * trajectory.RadiusAt(i * h));
  }
  const double revolutions = sum * h / 3.0;
  EXPECT_NEAR(trajectory.RadiusAt(kDurationS),
              kStartRadiusM + (revolutions * kPitchM), 1e-12);
  EXPECT_DOUBLE_EQ(trajectory.ArcLengthAt(kDurationS),
                   kVelocityMS * kDurationS);
}

// Cuts a constant carrier of `frequency` through S7 and S8.
std::vector<PitEdge> CutConstantCarrier(const DiscCutterConfig& config,
                                        double frequency,
                                        CutEdgeProfile profile = {}) {
  CarrierPhaseBlock carrier;
  for (std::int64_t m = 0; m < 20000; ++m) {
    const double t = static_cast<double>(m) / kModellingRateHz;
    carrier.phase_rad.push_back(2.0 * std::numbers::pi * frequency * t);
    carrier.frequency_hz.push_back(frequency);
    carrier.slice_level.push_back(0.0);
  }
  RecordingLimiter limiter;
  RecordedEdgeBlock recorded;
  limiter.Process(carrier, &recorded);
  DiscCutter cutter(config, profile);
  PitEdgeBlock cut;
  cutter.Process(recorded, &cut);
  EXPECT_EQ(cut.edges.size(), recorded.edges.size());
  for (std::size_t i = 0; i < cut.edges.size(); ++i) {
    EXPECT_EQ(cut.edges[i].polarity, recorded.edges[i].polarity);
    EXPECT_DOUBLE_EQ(cut.edges[i].radius_m,
                     cutter.Trajectory().RadiusAt(recorded.edges[i].time_s));
  }
  // A later edge can still move back by half the bias.
  EXPECT_DOUBLE_EQ(cut.complete_until_m,
                   cutter.Trajectory().ArcLengthAt(recorded.complete_until_s) -
                       (0.5 * profile.length_bias_m));
  return cut.edges;
}

// Plan Task 4.3 acceptance: the pit spatial frequency matches the IEC 60857
// Figure 18 note, V = f / (2πR·fr). At the 8.1 MHz blanking carrier and
// 55 mm that is 782.09 pits/mm, inside the HFMI band of 802 ± 26 pits/mm.
TEST(DiscCutterTest, CavPitSpatialFrequencyMatchesFigure18) {
  constexpr double kFrequencyHz = 8.1e6;
  const std::vector<PitEdge> edges =
      CutConstantCarrier(CavConfig(), kFrequencyHz);
  std::vector<const PitEdge*> starts;
  for (const PitEdge& edge : edges) {
    if (edge.polarity == EdgePolarity::kPitStart) {
      starts.push_back(&edge);
    }
  }
  ASSERT_GT(starts.size(), 1000U);
  for (std::size_t i = 1; i < starts.size(); ++i) {
    const double pits_per_mm =
        1e-3 / (starts[i]->arc_length_m - starts[i - 1]->arc_length_m);
    const double radius_mm =
        0.5e3 * (starts[i]->radius_m + starts[i - 1]->radius_m);
    const double figure18 = kFrequencyHz / (2.0 * std::numbers::pi * radius_mm *
                                            ntsc::kFrameRateHz);
    ASSERT_NEAR(pits_per_mm, figure18, 1e-6 * figure18) << "pit " << i;
  }
  EXPECT_NEAR(1e-3 / (starts[1]->arc_length_m - starts[0]->arc_length_m),
              782.09, 0.01);
}

// At the ideal setting every pit is half a carrier period of track, v/(2f);
// a length bias b lengthens every pit by b and shortens every land by b.
TEST(DiscCutterTest, LengthBiasLengthensEveryPit) {
  constexpr double kFrequencyHz = 8.1e6;
  constexpr double kBiasM = 0.1e-6;
  const std::vector<PitEdge> ideal =
      CutConstantCarrier(ClvConfig(), kFrequencyHz);
  const std::vector<PitEdge> biased =
      CutConstantCarrier(ClvConfig(), kFrequencyHz, {.length_bias_m = kBiasM});
  ASSERT_EQ(ideal.size(), biased.size());
  const double half_period = kVelocityMS / (2.0 * kFrequencyHz);
  for (std::size_t i = 1; i < ideal.size(); ++i) {
    const double ideal_run = ideal[i].arc_length_m - ideal[i - 1].arc_length_m;
    const double biased_run =
        biased[i].arc_length_m - biased[i - 1].arc_length_m;
    ASSERT_NEAR(ideal_run, half_period, 1e-12) << i;
    const bool pit = ideal[i].polarity == EdgePolarity::kPitEnd;
    ASSERT_NEAR(biased_run, half_period + (pit ? kBiasM : -kBiasM), 1e-12) << i;
  }
}

}  // namespace
}  // namespace ld_creator
