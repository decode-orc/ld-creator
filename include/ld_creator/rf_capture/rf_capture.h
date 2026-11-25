/*
 * File:        rf_capture.h
 * Module:      rf_capture
 * Purpose:     S13: filters, levels, samples and quantises the player RF
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "ld_creator/diagnostics/tap_writer.h"
#include "ld_creator/pipeline/stage_interfaces.h"
#include "ld_creator/project/project_config.h"
#include "ld_creator/rf_capture/capture_filter.h"

namespace ld_creator {

// How D9 is scaled to the ADC input u (±1 = the converter's range).
struct CaptureLevel {
  // The D9 level of a 50 %-duty pit train, subtracted before filtering:
  // 1 − m/2 (EdgeSumRenderer). It also stands for D9 before τ = 0 and after
  // the end of the run.
  double mean = 0.0;
  // u = scale·y, y the filtered D9.
  double scale = 0.0;
  // Peak-to-peak of the run's filtered D9 at the converter's samples, in D9
  // units (0 for the measuring pass).
  double peak_to_peak = 0.0;
};

// The D9 mean of a 50 %-duty pit train of contrast m: 1 − m/2. Pure.
double CaptureMean(double contrast);

// The level for a measuring pass: unit scale, so u = y.
CaptureLevel MeasuringCaptureLevel(double mean);

// The S13 level rule (S13 note, "Level"): the run's captured RF, measured
// at the converter's samples in a first pass with unit scale (lowest and
// highest y), has a peak-to-peak u of 2·level_fraction: the DdD guide's
// player RF peak-to-peak set to level_fraction of the converter's window.
// Pure function. Throws std::invalid_argument when highest ≤ lowest.
CaptureLevel LevelFromExtremes(const RfCaptureConfig& config, double mean,
                               double lowest, double highest);

// S13 rf_capture: D9 at Fr → D10 at 40 MHz (S13 note).
//   x[n] = D9[n] − mean, with x = 0 outside the run;
//   the chain's first-order sections, then its FIR, give y[n];
//   D10[n/2] = Q(scale·y[n]) for even n (exact decimation; S13 note,
//   "Sampling"), counting clipped codes.
// Each of `analogue_taps` (the rf_capture taps) receives u = scale·y at
// every Fr sample and is finished by Finish(); with none, the FIR is
// evaluated at even n only.
//
// Block-size independent. O(taps) per output. Ownership: owns the tap
// writers. Thread safety: one thread at a time.
class RfCapture final : public IRfCapture {
 public:
  RfCapture(CaptureProfile profile, CaptureChain chain, CaptureLevel level,
            std::vector<std::unique_ptr<SampledTapWriter>> analogue_taps = {});

  void Process(const PhotocurrentBlock& input, RfCodeBlock* output) override;
  void Finish(RfCodeBlock* output) override;

  [[nodiscard]] std::int64_t ClippedSamples() const { return clipped_; }
  // Extremes of u at the converter's samples so far whose FIR window lies
  // inside the run, away from the padding (+∞/−∞ before any).
  [[nodiscard]] double LowestSampled() const { return lowest_; }
  [[nodiscard]] double HighestSampled() const { return highest_; }

 private:
  void Emit(std::int64_t available_until, RfCodeBlock* output);

  CaptureProfile profile_;
  CaptureChain chain_;
  CaptureLevel level_;
  std::vector<std::unique_ptr<SampledTapWriter>> analogue_taps_;
  // Section states: previous input and output of each section.
  std::vector<double> section_input_;
  std::vector<double> section_output_;
  // Filtered-input history: history_[i] is x at index history_first_ + i.
  std::vector<double> history_;
  std::int64_t history_first_ = 0;
  std::int64_t received_ = 0;
  std::int64_t next_ = 0;
  std::int64_t clipped_ = 0;
  double lowest_ = std::numeric_limits<double>::infinity();
  double highest_ = -std::numeric_limits<double>::infinity();
  std::vector<double> tap_buffer_;
};

}  // namespace ld_creator
