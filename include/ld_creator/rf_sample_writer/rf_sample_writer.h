/*
 * File:        rf_sample_writer.h
 * Module:      rf_sample_writer
 * Purpose:     S14: writes captured RF codes as a signed 16-bit .s16 file
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>

#include "ld_creator/diagnostics/byte_sink.h"
#include "ld_creator/pipeline/stage_interfaces.h"
#include "ld_creator/project/project_config.h"

namespace ld_creator {

// The stored word for an RF code (S14 note):
//   domesday_duplicator: w = (c − 512)·64 for a 10-bit code c (DdD
//   capture-files.md; register S14.ddd_word);
//   ideal: w = c, a signed 16-bit code.
// Throws std::out_of_range for a code outside the profile's range.
std::int16_t RfWordForCode(CaptureProfile profile, std::int32_t code);

// S14 rf_sample_writer: D10 → little-endian signed 16-bit mono with no
// header (ld-decode's .s16 input; register S14.format).
//
// Thread safety: one thread at a time. Ownership: borrows `sink`.
class RfSampleWriter final : public IRfSampleSink {
 public:
  RfSampleWriter(CaptureProfile profile, IByteSink* sink);

  void Write(const RfCodeBlock& block) override;
  void Finish() override;

  [[nodiscard]] std::int64_t SamplesWritten() const { return written_; }

 private:
  CaptureProfile profile_;
  IByteSink* sink_;
  std::int64_t written_ = 0;
};

}  // namespace ld_creator
