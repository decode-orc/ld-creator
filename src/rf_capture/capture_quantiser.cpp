/*
 * File:        capture_quantiser.cpp
 * Module:      rf_capture
 * Purpose:     S13: the capture ADC's quantisation of its input to codes
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/rf_capture/capture_quantiser.h"

#include <cmath>

namespace ld_creator {

namespace {

// ADS825: 10-bit codes 0–1023, mid-scale 512 (S13 note, register
// S13.adc_bits).
constexpr double kDddMidCode = 512.0;
constexpr double kDddCodesPerHalfScale = 512.0;
constexpr double kDddMaxCode = 1023.0;

// The ideal profile: signed 16-bit codes (register S13.adc_bits).
constexpr double kIdealCodesPerHalfScale = 32768.0;
constexpr double kIdealMinCode = -32768.0;
constexpr double kIdealMaxCode = 32767.0;

QuantisedCode Clamp(double rounded, double low, double high) {
  if (rounded < low) {
    return {.code = static_cast<std::int32_t>(low), .clipped = true};
  }
  if (rounded > high) {
    return {.code = static_cast<std::int32_t>(high), .clipped = true};
  }
  return {.code = static_cast<std::int32_t>(rounded), .clipped = false};
}

}  // namespace

QuantisedCode QuantiseCaptureInput(CaptureProfile profile, double u) {
  if (profile == CaptureProfile::kIdeal) {
    return Clamp(std::round(kIdealCodesPerHalfScale * u), kIdealMinCode,
                 kIdealMaxCode);
  }
  return Clamp(std::round(kDddMidCode + (kDddCodesPerHalfScale * u)), 0.0,
               kDddMaxCode);
}

}  // namespace ld_creator
