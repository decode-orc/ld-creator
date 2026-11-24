/*
 * File:        cutting_trajectory.cpp
 * Module:      disc_cutter
 * Purpose:     S8: recording time to arc length and radius on the spiral
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/disc_cutter/cutting_trajectory.h"

#include <cmath>
#include <numbers>

#include "ld_creator/standards/ntsc_timing.h"

namespace ld_creator {

namespace {

constexpr double kMetresPerMm = 1e-3;
constexpr double kMetresPerUm = 1e-6;

}  // namespace

CuttingTrajectory::CuttingTrajectory(const DiscCutterConfig& config)
    : format_(config.format),
      start_radius_m_(config.start_radius_mm * kMetresPerMm),
      track_pitch_m_(config.track_pitch_um * kMetresPerUm),
      linear_velocity_m_s_(config.linear_velocity_m_s.value_or(0.0)) {}

double CuttingTrajectory::RadiusAt(double time_s) const {
  if (format_ == DiscFormat::kCav) {
    // IEC 60857-1986 4.7.1: one revolution, one pitch, per frame.
    return start_radius_m_ + (track_pitch_m_ * ntsc::kFrameRateHz * time_s);
  }
  // Area swept per unit time is p·v: 2π·r·dr = p·v·dt.
  return std::sqrt(
      (start_radius_m_ * start_radius_m_) +
      (track_pitch_m_ * linear_velocity_m_s_ * time_s / std::numbers::pi));
}

double CuttingTrajectory::ArcLengthAt(double time_s) const {
  if (format_ == DiscFormat::kCav) {
    // x = ∫ ω·r dt with ω = 2π·fr.
    return 2.0 * std::numbers::pi * ntsc::kFrameRateHz *
           ((start_radius_m_ * time_s) +
            (0.5 * track_pitch_m_ * ntsc::kFrameRateHz * time_s * time_s));
  }
  return linear_velocity_m_s_ * time_s;
}

}  // namespace ld_creator
