/*
 * File:        ntsc_timing.h
 * Module:      standards
 * Purpose:     Exact NTSC timing constants shared by the stages
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>

namespace ld_creator::ntsc {

// SMPTE 170M-2004: line frequency fH = 4.5 MHz / 286 (register S1.fh), as
// an exact ratio of integers and as a double.
inline constexpr std::int64_t kLineFrequencyNumeratorHz = 4'500'000;
inline constexpr std::int64_t kLineFrequencyDenominator = 286;
inline constexpr double kLineFrequencyHz =
    static_cast<double>(kLineFrequencyNumeratorHz) /
    static_cast<double>(kLineFrequencyDenominator);

// SMPTE 170M-2004: colour subcarrier fsc = 455/2 · fH (register S1.fsc).
inline constexpr double kSubcarrierFrequencyHz = 455.0 / 2.0 * kLineFrequencyHz;

// CVBS spec NTSC preset: sample rate 4fsc (register S1.sample_rate).
inline constexpr double kSampleRateHz = 4.0 * kSubcarrierFrequencyHz;

// SMPTE 170M-2004: 525 lines per frame.
inline constexpr std::int64_t kLinesPerFrame = 525;

// CVBS spec NTSC preset: 910 samples per line, orthogonal sampling
// (register S1.line_samples).
inline constexpr std::int64_t kSamplesPerLine = 910;

// CVBS spec NTSC preset: 525 × 910 = 477 750 samples per stored frame.
inline constexpr std::int64_t kSamplesPerFrame =
    kLinesPerFrame * kSamplesPerLine;

// SMPTE 170M-2004: frame rate fH / 525 = 30000/1001 Hz (register
// S1.frame_rate).
inline constexpr double kFrameRateHz = 30000.0 / 1001.0;

// CVBS spec NTSC preset, stored line alignment: 0H falls 57/90 of a sample
// period before stored sample 0 of each line (register S1.h_origin).
inline constexpr double kStoredOriginSamples = 57.0 / 90.0;

}  // namespace ld_creator::ntsc
