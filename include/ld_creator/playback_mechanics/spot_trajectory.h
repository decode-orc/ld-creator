/*
 * File:        spot_trajectory.h
 * Module:      playback_mechanics
 * Purpose:     The readout spot's position on the track over playback time
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

namespace ld_creator {

// Where the readout spot is at playback time τ (seconds from the D1 origin;
// S10 note): arc length x_s(τ) along the track, in metres, on the same
// scale as the D5–D7 pit edges, and the track speed dx_s/dτ in m/s.
// x_s is strictly increasing. The radial offset ρ(τ) joins this interface
// when tracking error is modelled (plan Task 8.1); the ideal player has
// ρ = 0.
//
// Implementations are immutable once built and safe to share between
// threads.
class ISpotTrajectory {
 public:
  virtual ~ISpotTrajectory() = default;

  [[nodiscard]] virtual double ArcLengthAt(double playback_time_s) const = 0;
  [[nodiscard]] virtual double SpeedAt(double playback_time_s) const = 0;

  // The inverse of ArcLengthAt: the playback time at which the spot reaches
  // `arc_length_m`.
  [[nodiscard]] virtual double TimeAtArcLength(double arc_length_m) const = 0;
};

}  // namespace ld_creator
