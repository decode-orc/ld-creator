/*
 * File:        rf_capture.cpp
 * Module:      rf_capture
 * Purpose:     S13: filters, levels, samples and quantises the player RF
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/rf_capture/rf_capture.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <utility>

#include "ld_creator/optical_readout/readout_kernel.h"
#include "ld_creator/rf_capture/capture_quantiser.h"

namespace ld_creator {

namespace {

// Fr / 40 MHz: the ADC takes every second readout sample.
constexpr std::int64_t kDecimation = 2;

// Filtered input kept beyond what the FIR still needs before it is
// discarded; affects memory only.
constexpr std::size_t kHistorySlack = std::size_t{1} << 16U;

}  // namespace

double CaptureMean(double contrast) { return 1.0 - (0.5 * contrast); }

CaptureLevel MeasuringCaptureLevel(double mean) {
  return {.mean = mean, .scale = 1.0, .peak_to_peak = 0.0};
}

CaptureLevel LevelFromExtremes(const RfCaptureConfig& config, double mean,
                               double lowest, double highest) {
  if (!(highest > lowest)) {
    throw std::invalid_argument(
        "The captured RF has no peak-to-peak to set the level from");
  }
  const double peak_to_peak = highest - lowest;
  return {.mean = mean,
          .scale = 2.0 * config.level_fraction / peak_to_peak,
          .peak_to_peak = peak_to_peak};
}

RfCapture::RfCapture(
    CaptureProfile profile, CaptureChain chain, CaptureLevel level,
    std::vector<std::unique_ptr<SampledTapWriter>> analogue_taps)
    : profile_(profile),
      chain_(std::move(chain)),
      level_(level),
      analogue_taps_(std::move(analogue_taps)),
      section_input_(chain_.sections.size(), 0.0),
      section_output_(chain_.sections.size(), 0.0) {
  // x = 0 before τ = 0: the oldest index the first output reads.
  const auto taps = static_cast<std::int64_t>(chain_.fir.taps.size());
  history_first_ = -chain_.fir.first_index - (taps - 1);
  // The filters' spans include tap 0, so the first output reads x[0].
  assert(history_first_ <= 0);
  history_.assign(static_cast<std::size_t>(-history_first_), 0.0);
}

void RfCapture::Process(const PhotocurrentBlock& input, RfCodeBlock* output) {
  for (const double sample : input.level) {
    double x = sample - level_.mean;
    for (std::size_t s = 0; s < chain_.sections.size(); ++s) {
      const FirstOrderSection& section = chain_.sections[s];
      const double y = (section.b0 * x) + (section.b1 * section_input_[s]) -
                       (section.a1 * section_output_[s]);
      section_input_[s] = x;
      section_output_[s] = y;
      x = y;
    }
    history_.push_back(x);
  }
  received_ += static_cast<std::int64_t>(input.level.size());
  // Output n reads x up to n − first_index.
  Emit(received_ - 1 + chain_.fir.first_index, output);
}

void RfCapture::Finish(RfCodeBlock* output) {
  // x = 0 after the end of the run.
  const std::int64_t lookahead =
      std::max<std::int64_t>(0, -chain_.fir.first_index);
  history_.insert(history_.end(), static_cast<std::size_t>(lookahead), 0.0);
  Emit(received_ - 1, output);
  for (const std::unique_ptr<SampledTapWriter>& tap : analogue_taps_) {
    tap->Finish();
  }
}

void RfCapture::Emit(std::int64_t available_until, RfCodeBlock* output) {
  output->codes.clear();
  output->first_sample = (next_ + kDecimation - 1) / kDecimation;
  const std::int64_t last = std::min(available_until, received_ - 1);
  const std::vector<double>& taps = chain_.fir.taps;
  const std::size_t count = taps.size();
  tap_buffer_.clear();
  const std::int64_t tap_first = next_;
  for (; next_ <= last; ++next_) {
    const bool sampled = next_ % kDecimation == 0;
    if (!sampled && analogue_taps_.empty()) {
      continue;
    }
    // y[n] = Σₖ taps[k]·x[n − first − k]; walk x downwards from n − first.
    const auto newest = static_cast<std::size_t>(
        next_ - chain_.fir.first_index - history_first_);
    double y = 0.0;
    for (std::size_t k = 0; k < count; ++k) {
      y += taps[k] * history_[newest - k];
    }
    const double u = level_.scale * y;
    if (!analogue_taps_.empty()) {
      tap_buffer_.push_back(u);
    }
    if (sampled) {
      // Extremes only where the FIR reads no padding (x = 0 before τ = 0
      // and after the run): the padding's start-up is not signal.
      const std::int64_t oldest =
          next_ - chain_.fir.first_index - static_cast<std::int64_t>(count - 1);
      const std::int64_t newest_index = next_ - chain_.fir.first_index;
      if (oldest >= 0 && newest_index < received_) {
        lowest_ = std::min(lowest_, u);
        highest_ = std::max(highest_, u);
      }
      const QuantisedCode code = QuantiseCaptureInput(profile_, u);
      output->codes.push_back(code.code);
      if (code.clipped) {
        ++clipped_;
      }
    }
  }
  if (!tap_buffer_.empty()) {
    for (const std::unique_ptr<SampledTapWriter>& tap : analogue_taps_) {
      tap->Write(tap_first, tap_buffer_);
    }
  }
  // Drop history the next output no longer reads.
  const std::int64_t oldest_needed =
      next_ - chain_.fir.first_index - static_cast<std::int64_t>(count - 1);
  const auto unused = oldest_needed - history_first_;
  if (unused > static_cast<std::int64_t>(kHistorySlack)) {
    history_.erase(history_.begin(),
                   history_.begin() + static_cast<std::ptrdiff_t>(unused));
    history_first_ += unused;
  }
}

}  // namespace ld_creator
