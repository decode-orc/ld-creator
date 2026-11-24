/*
 * File:        sampled_signal.h
 * Module:      pipeline
 * Purpose:     Uniformly sampled representations (D1–D3, D8–D10)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>
#include <vector>

namespace ld_creator {

// The sampling grid of a uniformly sampled stream. Sample n of the stream is
// at time (n + origin_samples) / rate_hz seconds, where time 0 is 0H of frame
// line 1 of the first input frame (docs-tech/user/disc-model/README.md, "Data
// representations"). Recording time t and playback time τ share this origin.
struct SampleClock {
  double rate_hz = 0.0;
  double origin_samples = 0.0;

  // Time of stream sample `index`, in seconds.
  [[nodiscard]] double TimeOf(std::int64_t index) const {
    return (static_cast<double>(index) + origin_samples) / rate_hz;
  }
};

// D1 and D2: composite video in IRE (100 IRE = blanking to white, IEC 60857
// §9.1.6 note), as double samples.
//   D1 (S1 output): 4fsc, origin 57/90 sample (S1 note).
//   D2 (S2–S4): the modelling rate Fm, origin 0 (S2 note).
// A block holds consecutive samples of the stream starting at stream index
// `first_sample`. Blocks of one stream are contiguous and in order.
struct IreSampleBlock {
  std::int64_t first_sample = 0;
  std::vector<double> ire;
};

// D3 (S5–S6): carrier phase φ in radians, its instantaneous frequency
// f = φ'/2π in Hz, and the slice level s (dimensionless, s ≡ 0 for video
// only), all at the modelling rate Fm with origin 0. The three vectors
// always have the same length. φ and f at each sample define the cubic
// Hermite interpolant of φ between samples (S5 note). A run of M D2 samples
// gives M + 1 D3 samples: the last is at the end of the run, so the
// interpolant covers the whole run.
struct CarrierPhaseBlock {
  std::int64_t first_sample = 0;
  std::vector<double> phase_rad;
  std::vector<double> frequency_hz;
  std::vector<double> slice_level;
};

// D8 (S11 output) and D9 (S12 output): detector photocurrent and player RF,
// normalised so unpitted land = 1, as double samples at the readout rate
// Fr = 80 MHz with origin 0 (playback time τ = n / Fr; S11 note).
struct PhotocurrentBlock {
  std::int64_t first_sample = 0;
  std::vector<double> level;
};

// D10 (S13 output): captured RF as integer ADC codes at 40 MHz, sample 0 at
// τ = 0 (S13 note). Codes are 0–1023 for the domesday_duplicator profile and
// signed 16-bit values for the ideal profile.
struct RfCodeBlock {
  std::int64_t first_sample = 0;
  std::vector<std::int32_t> codes;
};

}  // namespace ld_creator
