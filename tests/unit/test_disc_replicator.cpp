/*
 * File:        test_disc_replicator.cpp
 * Module:      tests
 * Purpose:     Unit tests for S9 and S10 at their ideal settings
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include "ld_creator/disc_cutter/cutting_trajectory.h"
#include "ld_creator/disc_replicator/disc_replicator.h"
#include "ld_creator/playback_mechanics/playback_mechanics.h"

namespace ld_creator {
namespace {

PitEdgeBlock SomeEdges() {
  PitEdgeBlock block;
  block.edges = {{.arc_length_m = 1e-6,
                  .radius_m = 0.055,
                  .polarity = EdgePolarity::kPitEnd},
                 {.arc_length_m = 1.6e-6,
                  .radius_m = 0.055,
                  .polarity = EdgePolarity::kPitStart}};
  block.complete_until_m = 2e-6;
  return block;
}

DiscCutterConfig Cav() {
  return {.format = DiscFormat::kCav,
          .start_radius_mm = 55.0,
          .track_pitch_um = 1.6667,
          .linear_velocity_m_s = std::nullopt};
}

DiscCutterConfig Clv() {
  return {.format = DiscFormat::kClv,
          .start_radius_mm = 55.0,
          .track_pitch_um = 1.6667,
          .linear_velocity_m_s = 11.0};
}

// Plan Task 5.1 acceptance: S9 is an exact copy.
TEST(ExactCopyReplicatorTest, PressedDiscEqualsTheCutPitMap) {
  ExactCopyReplicator replicator;
  const PitEdgeBlock input = SomeEdges();
  PitEdgeBlock output;
  replicator.Process(input, &output);
  ASSERT_EQ(output.edges.size(), input.edges.size());
  for (std::size_t i = 0; i < input.edges.size(); ++i) {
    EXPECT_EQ(output.edges[i].arc_length_m, input.edges[i].arc_length_m);
    EXPECT_EQ(output.edges[i].radius_m, input.edges[i].radius_m);
    EXPECT_EQ(output.edges[i].polarity, input.edges[i].polarity);
  }
  EXPECT_EQ(output.complete_until_m, input.complete_until_m);
  replicator.Finish(&output);
  EXPECT_TRUE(output.edges.empty());
  EXPECT_EQ(output.complete_until_m, input.complete_until_m);
}

// Plan Task 5.1 acceptance: the spot follows the S8 trajectory.
TEST(LockedPlaybackMechanicsTest, SpotTrajectoryEqualsTheCuttingTrajectory) {
  for (const DiscCutterConfig& config : {Cav(), Clv()}) {
    const CuttingTrajectory cutting(config);
    const LockedPlaybackMechanics mechanics(cutting);
    for (double tau = 0.0; tau < 100.0; tau += 3.7) {
      EXPECT_EQ(mechanics.Trajectory().ArcLengthAt(tau),
                cutting.ArcLengthAt(tau));
      EXPECT_EQ(mechanics.Trajectory().SpeedAt(tau), cutting.SpeedAt(tau));
    }
  }
}

TEST(LockedPlaybackMechanicsTest, PassesEdgesAndConvertsTheWatermark) {
  const CuttingTrajectory cutting(Cav());
  LockedPlaybackMechanics mechanics(cutting);
  const PitEdgeBlock input = SomeEdges();
  ScanBlock output;
  mechanics.Process(input, &output);
  ASSERT_EQ(output.edges.size(), input.edges.size());
  EXPECT_EQ(output.edges[1].arc_length_m, input.edges[1].arc_length_m);
  EXPECT_NEAR(cutting.ArcLengthAt(output.complete_until_s),
              input.complete_until_m, 1e-18);
}

TEST(CuttingTrajectoryTest, TimeAtArcLengthInvertsArcLength) {
  for (const DiscCutterConfig& config : {Cav(), Clv()}) {
    const CuttingTrajectory cutting(config);
    for (double t = 0.0; t < 3600.0; t += 61.3) {
      const double x = cutting.ArcLengthAt(t);
      EXPECT_NEAR(cutting.TimeAtArcLength(x), t, 1e-12 * (1.0 + t)) << t;
    }
  }
}

TEST(CuttingTrajectoryTest, SpeedIsTheDerivativeOfArcLength) {
  for (const DiscCutterConfig& config : {Cav(), Clv()}) {
    const CuttingTrajectory cutting(config);
    for (double t = 1.0; t < 1000.0; t += 97.0) {
      const double step = 1e-3;
      const double numeric =
          (cutting.ArcLengthAt(t + step) - cutting.ArcLengthAt(t - step)) /
          (2.0 * step);
      EXPECT_NEAR(cutting.SpeedAt(t), numeric, 1e-7 * numeric);
    }
  }
}

}  // namespace
}  // namespace ld_creator
