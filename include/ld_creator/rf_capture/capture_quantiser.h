/*
 * File:        capture_quantiser.h
 * Module:      rf_capture
 * Purpose:     S13: the capture ADC's quantisation of its input to codes
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>

#include "ld_creator/project/project_config.h"

namespace ld_creator {

// S13 capture sample rate, 40 MHz (register S13.sample_rate).
inline constexpr std::int64_t kCaptureRateHz = 40'000'000;

struct QuantisedCode {
  std::int32_t code = 0;
  bool clipped = false;
};

// Q(u) for an ADC input u relative to half the full scale (±1 = the ends of
// the converter's window), S13 note "Quantisation (declared)":
//   domesday_duplicator: clamp(round(512 + 512·u), 0, 1023)
//   ideal:               clamp(round(32768·u), −32768, 32767)
// round() is to nearest, halves away from zero. `clipped` is set when the
// clamp changed the code. Pure function.
QuantisedCode QuantiseCaptureInput(CaptureProfile profile, double u);

}  // namespace ld_creator
