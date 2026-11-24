/*
 * File:        rf_sample_writer.cpp
 * Module:      rf_sample_writer
 * Purpose:     S14: writes captured RF codes as a signed 16-bit .s16 file
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/rf_sample_writer/rf_sample_writer.h"

#include <cassert>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace ld_creator {

namespace {

// DomesdayDuplicator capture-files.md: 10-bit ADC codes 0–1023, centred on
// 512 and scaled by 64 in the 16-bit word (register S14.ddd_word).
constexpr std::int32_t kDddMaximumCode = 1023;
constexpr std::int32_t kDddCentreCode = 512;
constexpr std::int32_t kDddWordScale = 64;

}  // namespace

std::int16_t RfWordForCode(CaptureProfile profile, std::int32_t code) {
  if (profile == CaptureProfile::kDomesdayDuplicator) {
    if (code < 0 || code > kDddMaximumCode) {
      throw std::out_of_range("RF code " + std::to_string(code) +
                              " is outside the 10-bit range 0–1023");
    }
    return static_cast<std::int16_t>((code - kDddCentreCode) * kDddWordScale);
  }
  if (code < std::numeric_limits<std::int16_t>::min() ||
      code > std::numeric_limits<std::int16_t>::max()) {
    throw std::out_of_range("RF code " + std::to_string(code) +
                            " is outside the signed 16-bit range");
  }
  return static_cast<std::int16_t>(code);
}

RfSampleWriter::RfSampleWriter(CaptureProfile profile, IByteSink* sink)
    : profile_(profile), sink_(sink) {
  assert(sink_ != nullptr);
}

void RfSampleWriter::Write(const RfCodeBlock& block) {
  if (block.first_sample != written_) {
    throw std::logic_error("RF block starts at sample " +
                           std::to_string(block.first_sample) + ", expected " +
                           std::to_string(written_));
  }
  std::vector<std::byte> bytes;
  bytes.reserve(block.codes.size() * 2);
  for (const std::int32_t code : block.codes) {
    const auto word = static_cast<std::uint16_t>(RfWordForCode(profile_, code));
    bytes.push_back(static_cast<std::byte>(word & 0xFF));
    bytes.push_back(static_cast<std::byte>(word >> 8));
  }
  sink_->Write(bytes);
  written_ += static_cast<std::int64_t>(block.codes.size());
}

void RfSampleWriter::Finish() { sink_->Close(); }

}  // namespace ld_creator
