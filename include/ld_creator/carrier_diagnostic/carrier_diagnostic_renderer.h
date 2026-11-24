/*
 * File:        carrier_diagnostic_renderer.h
 * Module:      carrier_diagnostic
 * Purpose:     Renders cos φ at the capture rate in carrier diagnostic mode
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>
#include <vector>

#include "ld_creator/pipeline/stage_interfaces.h"
#include "ld_creator/project/project_config.h"

namespace ld_creator {

// D3 sample position of capture sample n is n·P/Q Fm samples, with
// P/Q = Fm / 40 MHz in lowest terms (189/88).
struct CaptureToModellingRatio {
  std::int64_t numerator = 0;
  std::int64_t denominator = 0;
};
CaptureToModellingRatio CaptureToModellingRate();

// The carrier diagnostic (output.mode carrier_diagnostic). NOT A DISC: it
// skips S6–S12 and renders the modulated carrier directly, so that a fault
// can be placed before or after the recording limiter (plan, "Carrier
// diagnostic").
//
// Capture sample n is at τ_n = n / 40 MHz. φ(τ_n) is the cubic Hermite
// interpolant of the D3 samples on either side (S5 note), and the ADC input
// is u = level_fraction · cos φ(τ_n): the carrier's peak-to-peak amplitude
// is level_fraction of full scale, the S13 level rule applied to a carrier
// of constant amplitude. u is quantised by the S13 profile's Q. Samples are
// produced for every τ_n before the end of the run (the last D3 sample).
//
// O(1) per output sample. Block-size independent. Thread safety: one thread
// at a time.
class CarrierDiagnosticRenderer final
    : public IBlockStage<CarrierPhaseBlock, RfCodeBlock> {
 public:
  explicit CarrierDiagnosticRenderer(const RfCaptureConfig& capture);

  void Process(const CarrierPhaseBlock& input, RfCodeBlock* output) override;
  void Finish(RfCodeBlock* output) override;

  // Samples whose code was clamped at the end of the converter's range.
  [[nodiscard]] std::int64_t ClippedSamples() const { return clipped_; }

 private:
  void Emit(RfCodeBlock* output);

  CaptureProfile profile_;
  double level_;
  CaptureToModellingRatio ratio_;
  std::vector<double> phase_;
  std::vector<double> frequency_;
  std::int64_t history_first_ = 0;
  std::int64_t received_ = 0;
  std::int64_t next_output_ = 0;
  std::int64_t clipped_ = 0;
};

}  // namespace ld_creator
