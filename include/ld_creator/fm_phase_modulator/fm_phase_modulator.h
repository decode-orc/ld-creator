/*
 * File:        fm_phase_modulator.h
 * Module:      fm_phase_modulator
 * Purpose:     S5: carrier phase of the IEC 60857 9.2 frequency-modulated video
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "ld_creator/pipeline/stage_interfaces.h"
#include "ld_creator/project/project_config.h"

namespace ld_creator {

// IEC 60857-1986 9.2.3: the deviation is specified from sync tip (−40 IRE)
// to white (+100 IRE), a span of 140 IRE (register S5.deviation_span).
inline constexpr double kDeviationSpanIre = 140.0;

// Integration rule for one Fm interval (register S5.integration_rule): the
// integral over [t_m, t_m+1] of the degree-5 polynomial through samples
// m−2 … m+3, ∫ = h·Σⱼ cⱼ·f[m−2+j] with cⱼ = kIntegrationWeights[j] / 1440.
// Exact for polynomials up to degree 5; its frequency response is given in
// the S5 note.
inline constexpr std::array<double, 6> kIntegrationWeights = {
    11.0, -93.0, 802.0, 802.0, -93.0, 11.0};
inline constexpr double kIntegrationDivisor = 1440.0;

// S5 fm_phase_modulator: D2 → D3.
//   f(t) = f_b + k·v(t),  k = Δf_dev / 140 IRE   (IEC 60857-1986 9.2.1–9.2.3)
//   φ(t_m) = φ₀ + 2π ∫₀^{t_m} f
// The phase is accumulated in turns as a whole number of cycles plus a
// fraction in [0, 1), so its rounding error does not grow with run length.
// D3 sample m carries φ(t_m), f(t_m) and s = 0. The input is 0 IRE outside
// the run, and the output has one sample more than the input (D3 contract).
//
// O(6) per sample. Block-size independent. Thread safety: one thread at a
// time.
class FmPhaseModulator final : public IFmPhaseModulator {
 public:
  explicit FmPhaseModulator(const FmPhaseModulatorConfig& config);

  void Process(const IreSampleBlock& input, CarrierPhaseBlock* output) override;
  void Finish(CarrierPhaseBlock* output) override;

 private:
  void Emit(bool at_end, CarrierPhaseBlock* output);
  [[nodiscard]] double InputAt(std::int64_t index) const;

  double blanking_hz_;
  double hz_per_ire_;
  double initial_phase_rad_;
  // Turns per Fm interval at blanking, and per IRE.
  double blanking_turns_;
  double turns_per_ire_;

  std::vector<double> history_;
  std::int64_t history_first_ = 0;
  std::int64_t input_count_ = 0;
  std::int64_t next_output_ = 0;
  // (φ_n − φ₀)/2π = cycles_ + fraction_, for n = accumulated_index_.
  std::int64_t accumulated_index_ = 0;
  std::int64_t cycles_ = 0;
  double fraction_ = 0.0;
};

}  // namespace ld_creator
