/*
 * File:        recording_limiter.h
 * Module:      recording_limiter
 * Purpose:     S7: pit-edge times where the limited carrier changes level
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>

#include "ld_creator/pipeline/stage_interfaces.h"

namespace ld_creator {

// The largest ratio of an end slope of the φ interpolant to its secant for
// which a cubic Hermite segment is monotone (Fritsch and Carlson 1980). The
// limiter requires both end slopes in (0, 3] × secant, so each target phase
// is crossed exactly once in an interval (S7 note, "Root finding").
inline constexpr double kMonotoneSlopeRatio = 3.0;

// Newton iterations allowed per edge before the solver stops (S7 note). The
// near-linear interpolant converges in 3 or 4; the cap only bounds the
// bisection fallback, which halves the bracket each step.
inline constexpr int kMaximumEdgeIterations = 64;

// S7 recording_limiter: D3 → D4. An ideal limiter cuts a pit while
// cos φ(t) > s(t) (register S7.pit_polarity). With s ≡ 0 the edges are the
// solutions of φ(t_k) = (k + ½)π: even k ends a pit, odd k starts one.
//
// Each crossing is solved on the cubic Hermite interpolant of φ between D3
// samples (S5 note) by Newton's method inside the bracket [0, 1] of the
// interval, falling back to bisection when a Newton step leaves the bracket.
// The edge-time error bound is in the S7 note.
//
// Throws std::invalid_argument on a non-zero slice level (audio and EFM are
// not implemented), and std::runtime_error if φ does not increase
// monotonically across an interval.
//
// O(1) per sample plus O(1) per edge. Block-size independent. Thread safety:
// one thread at a time.
class RecordingLimiter final : public IRecordingLimiter {
 public:
  void Process(const CarrierPhaseBlock& input,
               RecordedEdgeBlock* output) override;
  void Finish(RecordedEdgeBlock* output) override;

 private:
  void CutInterval(double phase1_rad, double frequency1_hz,
                   RecordedEdgeBlock* output);

  bool started_ = false;
  // The previous D3 sample: the start of the next interval.
  std::int64_t previous_index_ = 0;
  double previous_phase_rad_ = 0.0;
  double previous_frequency_hz_ = 0.0;
  // The next crossing is at φ = (next_k_ + ½)π.
  std::int64_t next_k_ = 0;
};

}  // namespace ld_creator
