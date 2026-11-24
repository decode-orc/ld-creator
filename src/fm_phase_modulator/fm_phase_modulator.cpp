/*
 * File:        fm_phase_modulator.cpp
 * Module:      fm_phase_modulator
 * Purpose:     S5: carrier phase of the IEC 60857 9.2 frequency-modulated video
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/fm_phase_modulator/fm_phase_modulator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "ld_creator/dsp/portable_math.h"
#include "ld_creator/video_rate_converter/video_rate_converter.h"

namespace ld_creator {

namespace {

// The rule reaches 2 samples back and 3 forward from the interval start.
constexpr std::int64_t kRuleBack = 2;
constexpr std::int64_t kRuleForward = 3;

}  // namespace

FmPhaseModulator::FmPhaseModulator(const FmPhaseModulatorConfig& config)
    : blanking_hz_(config.blanking_hz),
      hz_per_ire_(config.deviation_hz / kDeviationSpanIre),
      initial_phase_rad_(config.initial_phase_rad),
      blanking_turns_(config.blanking_hz / kModellingRateHz),
      turns_per_ire_(config.deviation_hz / kDeviationSpanIre /
                     kModellingRateHz / kIntegrationDivisor) {}

void FmPhaseModulator::Process(const IreSampleBlock& input,
                               CarrierPhaseBlock* output) {
  history_.insert(history_.end(), input.ire.begin(), input.ire.end());
  input_count_ += static_cast<std::int64_t>(input.ire.size());
  Emit(/*at_end=*/false, output);
}

void FmPhaseModulator::Finish(CarrierPhaseBlock* output) {
  Emit(/*at_end=*/true, output);
}

double FmPhaseModulator::InputAt(std::int64_t index) const {
  if (index < 0 || index >= input_count_) {
    return 0.0;
  }
  return history_[static_cast<std::size_t>(index - history_first_)];
}

void FmPhaseModulator::Emit(bool at_end, CarrierPhaseBlock* output) {
  output->first_sample = next_output_;
  output->phase_rad.clear();
  output->frequency_hz.clear();
  output->slice_level.clear();
  // Samples 0 … M, M = input samples, the last only once the input has ended.
  const std::int64_t last_output = at_end ? input_count_ : input_count_ - 1;
  while (next_output_ <= last_output) {
    // Bring the accumulator up to φ_next by adding interval increments.
    bool ready = true;
    while (accumulated_index_ < next_output_) {
      const std::int64_t start = accumulated_index_;
      if (!at_end && start + kRuleForward >= input_count_) {
        ready = false;
        break;
      }
      double weighted = 0.0;
      for (std::int64_t j = 0;
           j < static_cast<std::int64_t>(kIntegrationWeights.size()); ++j) {
        weighted += kIntegrationWeights[static_cast<std::size_t>(j)] *
                    InputAt(start - kRuleBack + j);
      }
      fraction_ += blanking_turns_ + (turns_per_ire_ * weighted);
      const double whole = std::floor(fraction_);
      cycles_ += static_cast<std::int64_t>(whole);
      fraction_ -= whole;
      ++accumulated_index_;
    }
    if (!ready) {
      break;
    }
    output->phase_rad.push_back(
        initial_phase_rad_ +
        (kTwoPi * (static_cast<double>(cycles_) + fraction_)));
    output->frequency_hz.push_back(blanking_hz_ +
                                   (hz_per_ire_ * InputAt(next_output_)));
    output->slice_level.push_back(0.0);
    ++next_output_;
  }
  // The next increment starts at accumulated_index_ and reaches 2 back.
  const std::int64_t keep_from = std::max<std::int64_t>(
      0, std::min(accumulated_index_ - kRuleBack, next_output_));
  if (keep_from > history_first_) {
    const std::int64_t drop = std::min<std::int64_t>(
        keep_from - history_first_, static_cast<std::int64_t>(history_.size()));
    history_.erase(history_.begin(),
                   history_.begin() + static_cast<std::ptrdiff_t>(drop));
    history_first_ += drop;
  }
}

}  // namespace ld_creator
