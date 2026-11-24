/*
 * File:        carrier_diagnostic_renderer.cpp
 * Module:      carrier_diagnostic
 * Purpose:     Renders cos φ at the capture rate in carrier diagnostic mode
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/carrier_diagnostic/carrier_diagnostic_renderer.h"

#include <algorithm>
#include <cstddef>
#include <numeric>

#include "ld_creator/dsp/hermite_phase.h"
#include "ld_creator/dsp/portable_math.h"
#include "ld_creator/rf_capture/capture_quantiser.h"
#include "ld_creator/standards/ntsc_timing.h"
#include "ld_creator/video_rate_converter/video_rate_converter.h"

namespace ld_creator {

CaptureToModellingRatio CaptureToModellingRate() {
  // Fm = 6 · 910 · fH = 6 · 910 · 4.5 MHz / 286.
  const std::int64_t numerator = kModellingRateMultiple *
                                 ntsc::kSamplesPerLine *
                                 ntsc::kLineFrequencyNumeratorHz;
  const std::int64_t denominator =
      ntsc::kLineFrequencyDenominator * kCaptureRateHz;
  const std::int64_t divisor = std::gcd(numerator, denominator);
  return {.numerator = numerator / divisor,
          .denominator = denominator / divisor};
}

CarrierDiagnosticRenderer::CarrierDiagnosticRenderer(
    const RfCaptureConfig& capture)
    : profile_(capture.profile),
      level_(capture.level_fraction),
      ratio_(CaptureToModellingRate()) {}

void CarrierDiagnosticRenderer::Process(const CarrierPhaseBlock& input,
                                        RfCodeBlock* output) {
  phase_.insert(phase_.end(), input.phase_rad.begin(), input.phase_rad.end());
  frequency_.insert(frequency_.end(), input.frequency_hz.begin(),
                    input.frequency_hz.end());
  received_ += static_cast<std::int64_t>(input.phase_rad.size());
  Emit(output);
}

void CarrierDiagnosticRenderer::Finish(RfCodeBlock* output) { Emit(output); }

void CarrierDiagnosticRenderer::Emit(RfCodeBlock* output) {
  output->first_sample = next_output_;
  output->codes.clear();
  constexpr double kModellingIntervalS = 1.0 / kModellingRateHz;
  while (true) {
    const std::int64_t position = next_output_ * ratio_.numerator;
    const std::int64_t index = position / ratio_.denominator;
    // Needs D3 samples index and index + 1; the last D3 sample is the end
    // of the run, so this also stops at the end.
    if (index + 1 >= received_) {
      break;
    }
    const double u = static_cast<double>(position % ratio_.denominator) /
                     static_cast<double>(ratio_.denominator);
    const auto local = static_cast<std::size_t>(index - history_first_);
    const double phase =
        phase_[local] + HermitePhaseIncrement(phase_[local + 1] - phase_[local],
                                              frequency_[local],
                                              frequency_[local + 1],
                                              kModellingIntervalS, u);
    const QuantisedCode code =
        QuantiseCaptureInput(profile_, level_ * CosTurns(phase / kTwoPi));
    output->codes.push_back(code.code);
    if (code.clipped) {
      ++clipped_;
    }
    ++next_output_;
  }
  const std::int64_t keep_from = std::min(
      received_, (next_output_ * ratio_.numerator) / ratio_.denominator);
  if (keep_from > history_first_) {
    const auto drop = static_cast<std::ptrdiff_t>(keep_from - history_first_);
    phase_.erase(phase_.begin(), phase_.begin() + drop);
    frequency_.erase(frequency_.begin(), frequency_.begin() + drop);
    history_first_ = keep_from;
  }
}

}  // namespace ld_creator
