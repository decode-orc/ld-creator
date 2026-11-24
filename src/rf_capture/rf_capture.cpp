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
#include <numbers>
#include <utility>

#include "ld_creator/dsp/portable_math.h"
#include "ld_creator/optical_readout/readout_kernel.h"
#include "ld_creator/rf_capture/capture_quantiser.h"

namespace ld_creator {

namespace {

// Points per period on which the reference waveform's extremes are found:
// the relative error of the peak-to-peak is below (π/4096)²/2·k² for
// harmonic k, under 1e-6 for the fundamental.
constexpr int kReferencePoints = 4096;

// Fr / 40 MHz: the ADC takes every second readout sample.
constexpr std::int64_t kDecimation = 2;

// Filtered input kept beyond what the FIR still needs before it is
// discarded; affects memory only.
constexpr std::size_t kHistorySlack = std::size_t{1} << 16U;

}  // namespace

CaptureLevel ComputeCaptureLevel(
    const RfCaptureConfig& config, const CaptureChain& chain,
    const std::function<double(double)>& readout_transfer, double contrast,
    double blanking_hz, double start_speed_m_s) {
  struct Harmonic {
    int order;
    ComplexValue weight;
  };
  std::vector<Harmonic> harmonics;
  for (int k = 1; static_cast<double>(k) * blanking_hz < 0.5 * kReadoutRateHz;
       k += 2) {
    const double frequency = static_cast<double>(k) * blanking_hz;
    const double optics = readout_transfer(frequency / start_speed_m_s);
    const double amplitude =
        -contrast * 2.0 / (std::numbers::pi * static_cast<double>(k)) * optics;
    harmonics.push_back(
        {.order = k, .weight = amplitude * chain.ResponseAt(frequency)});
  }
  double highest = -1e300;
  double lowest = 1e300;
  for (int j = 0; j < kReferencePoints; ++j) {
    double y = 0.0;
    for (const Harmonic& harmonic : harmonics) {
      const SinCosValue phase = SinCosTurns(
          static_cast<double>(harmonic.order) * static_cast<double>(j) /
          static_cast<double>(kReferencePoints));
      // Im[w·e^(iθ)].
      y += (harmonic.weight.re * phase.sin) + (harmonic.weight.im * phase.cos);
    }
    highest = std::max(highest, y);
    lowest = std::min(lowest, y);
  }
  CaptureLevel level;
  level.mean = 1.0 - (0.5 * contrast);
  level.reference_peak_to_peak = highest - lowest;
  level.scale = 2.0 * config.level_fraction / level.reference_peak_to_peak;
  return level;
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
