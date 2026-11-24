/*
 * File:        playback_mechanics.h
 * Module:      playback_mechanics
 * Purpose:     S10: the spot's path over the pressed disc during playback
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include "ld_creator/disc_cutter/cutting_trajectory.h"
#include "ld_creator/pipeline/stage_interfaces.h"
#include "ld_creator/playback_mechanics/spot_trajectory.h"

namespace ld_creator {

// The ideal player's spot trajectory: locked to the disc and tracking
// perfectly, so x_s(τ) = x(τ), the S8 cutting trajectory evaluated at the
// playback time, and ρ = 0 (S10 note; register S10.lock).
class LockedSpotTrajectory final : public ISpotTrajectory {
 public:
  explicit LockedSpotTrajectory(CuttingTrajectory cutting)
      : cutting_(cutting) {}

  [[nodiscard]] double ArcLengthAt(double playback_time_s) const override {
    return cutting_.ArcLengthAt(playback_time_s);
  }
  [[nodiscard]] double SpeedAt(double playback_time_s) const override {
    return cutting_.SpeedAt(playback_time_s);
  }
  [[nodiscard]] double TimeAtArcLength(double arc_length_m) const override {
    return cutting_.TimeAtArcLength(arc_length_m);
  }

 private:
  CuttingTrajectory cutting_;
};

// S10 playback_mechanics at its ideal setting: D6 → D7. The edges pass
// unchanged; the D6 arc-length watermark becomes the playback time at which
// the spot reaches it. Phase 8 impairments (eccentricity, speed variation)
// implement the same interface with another trajectory.
//
// O(1) per edge. Block-size independent. Thread safety: one thread at a
// time; Trajectory() may be shared.
class LockedPlaybackMechanics final : public IPlaybackMechanics {
 public:
  explicit LockedPlaybackMechanics(const CuttingTrajectory& cutting)
      : trajectory_(cutting) {}

  void Process(const PitEdgeBlock& input, ScanBlock* output) override;
  void Finish(ScanBlock* output) override;

  // The spot trajectory x_s(τ), which S11 and S12 read (S10 note,
  // "Input/output contract").
  [[nodiscard]] const ISpotTrajectory& Trajectory() const {
    return trajectory_;
  }

 private:
  LockedSpotTrajectory trajectory_;
  double complete_until_s_ = 0.0;
};

}  // namespace ld_creator
