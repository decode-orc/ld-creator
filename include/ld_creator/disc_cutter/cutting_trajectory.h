/*
 * File:        cutting_trajectory.h
 * Module:      disc_cutter
 * Purpose:     S8: recording time to arc length and radius on the spiral
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include "ld_creator/project/project_config.h"

namespace ld_creator {

// The cutting spot's path along a perfect spiral (S8 note, "Model and
// equations"). Time t is recording time in seconds from the D1 origin; arc
// length x is in metres along the track from the cutting position at t = 0;
// radius r is in metres.
//
//   CAV (IEC 60857-1986 4.7.1): one revolution per frame, ω = 2π·fr,
//     r(t) = r₀ + p·fr·t,   x(t) = 2π·fr·(r₀·t + p·fr·t²/2)
//   CLV (IEC 60857-1986 4.7.2): constant linear velocity v,
//     r(t) = √(r₀² + p·v·t/π),   x(t) = v·t
//
// with p the track pitch (4.15) and fr = 30000/1001 Hz (register
// S8.cav_rotation). Pure functions of t: immutable and safe to share
// between threads.
class CuttingTrajectory {
 public:
  explicit CuttingTrajectory(const DiscCutterConfig& config);

  [[nodiscard]] double RadiusAt(double time_s) const;
  [[nodiscard]] double ArcLengthAt(double time_s) const;

 private:
  DiscFormat format_;
  double start_radius_m_;
  double track_pitch_m_;
  double linear_velocity_m_s_;
};

}  // namespace ld_creator
