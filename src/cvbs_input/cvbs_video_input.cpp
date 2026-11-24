/*
 * File:        cvbs_video_input.cpp
 * Module:      cvbs_input
 * Purpose:     S1: reads NTSC .cvbs samples as IRE (D1)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/cvbs_input/cvbs_video_input.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <vector>

#include "ld_creator/standards/ntsc_timing.h"

namespace ld_creator {

namespace {

// CVBS spec, CVBS_U16_4FSC and CVBS_TPG21_4FSC: ×64 storage scale.
constexpr double kScale64 = 64.0;
// CVBS spec, CVBS_S16_4FSC: ×32 storage scale about blanking.
constexpr double kScale32 = 32.0;
// CVBS spec, CVBS_TPG21_4FSC: fixed conversion offset 508.
constexpr double kTpg21Offset = 508.0;
// IEC 60857-1986 9.1.6 note: 100 IRE is the blanking-to-white amplitude
// (register S1.ire_scale).
constexpr double kIrePerWhite = 100.0;

constexpr std::int64_t kFrameBytes =
    ntsc::kSamplesPerFrame * kBytesPerSampleWord;
constexpr std::int64_t kLineBytes = ntsc::kSamplesPerLine * kBytesPerSampleWord;

double TenBitValue(SampleEncoding encoding, std::uint16_t word) {
  const auto signed_word = static_cast<std::int16_t>(word);
  switch (encoding) {
    case SampleEncoding::kU10:
      // Signed 16-bit container holding the 10-bit value directly.
      return signed_word;
    case SampleEncoding::kU16:
      return static_cast<double>(word) / kScale64;
    case SampleEncoding::kS16:
      return static_cast<double>(signed_word) / kScale32 + kNtscBlankingCode;
    case SampleEncoding::kTpg21:
      return static_cast<double>(signed_word) / kScale64 + kTpg21Offset;
  }
  return 0.0;
}

}  // namespace

void DecodeSampleWords(SampleEncoding encoding,
                       std::span<const std::byte> bytes,
                       std::span<double> ire) {
  assert(bytes.size() == ire.size() * kBytesPerSampleWord);
  constexpr double kIrePerCode =
      kIrePerWhite / (kNtscWhiteCode - kNtscBlankingCode);
  for (std::size_t index = 0; index < ire.size(); ++index) {
    const auto low = static_cast<std::uint16_t>(bytes[2 * index]);
    const auto high = static_cast<std::uint16_t>(bytes[2 * index + 1]);
    const auto word = static_cast<std::uint16_t>(low | (high << 8));
    ire[index] =
        (TenBitValue(encoding, word) - kNtscBlankingCode) * kIrePerCode;
  }
}

FileByteSource::FileByteSource(const std::string& path)
    : path_(path), stream_(path, std::ios::binary) {
  if (!stream_) {
    throw std::runtime_error("cannot open input video " + path);
  }
  stream_.seekg(0, std::ios::end);
  size_ = static_cast<std::int64_t>(stream_.tellg());
}

std::int64_t FileByteSource::Size() const { return size_; }

void FileByteSource::Read(std::int64_t offset, std::span<std::byte> bytes) {
  stream_.clear();
  stream_.seekg(offset);
  stream_.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  if (stream_.gcount() != static_cast<std::streamsize>(bytes.size())) {
    throw std::runtime_error("short read from input video " + path_);
  }
}

FrameCountResult CountInputFrames(const CvbsMetadata& metadata,
                                  std::int64_t file_bytes) {
  const std::int64_t whole_frames = file_bytes / kFrameBytes;
  const std::int64_t remainder = file_bytes % kFrameBytes;
  if (remainder != 0) {
    return {.frames = std::nullopt,
            .error = "input frame " + std::to_string(whole_frames) +
                     " is truncated: " + std::to_string(remainder) + " of " +
                     std::to_string(kFrameBytes) + " bytes present"};
  }
  if (metadata.number_of_sequential_frames &&
      *metadata.number_of_sequential_frames != whole_frames) {
    const std::int64_t declared = *metadata.number_of_sequential_frames;
    if (declared > whole_frames) {
      return {.frames = std::nullopt,
              .error = "input frame " + std::to_string(whole_frames) +
                       " is missing: the metadata declares " +
                       std::to_string(declared) + " frames, the file holds " +
                       std::to_string(whole_frames)};
    }
    return {.frames = std::nullopt,
            .error = "the input file holds " + std::to_string(whole_frames) +
                     " frames, but the metadata declares " +
                     std::to_string(declared)};
  }
  if (whole_frames == 0) {
    return {.frames = std::nullopt, .error = "the input file holds no frames"};
  }
  return {.frames = whole_frames, .error = {}};
}

CvbsVideoInput::CvbsVideoInput(IByteSource* source, SampleEncoding encoding,
                               std::int64_t first_frame,
                               std::int64_t frame_count,
                               std::int64_t lines_per_block)
    : source_(source),
      encoding_(encoding),
      first_frame_(first_frame),
      frame_count_(frame_count),
      lines_per_block_(lines_per_block) {
  assert(source_ != nullptr);
  assert(first_frame_ >= 0 && frame_count_ >= 0);
  assert(lines_per_block_ >= 1);
}

VideoInputStream CvbsVideoInput::Stream() const {
  return VideoInputStream{
      .clock = SampleClock{.rate_hz = ntsc::kSampleRateHz,
                           .origin_samples = ntsc::kStoredOriginSamples},
      .frame_count = frame_count_};
}

bool CvbsVideoInput::ReadBlock(IreSampleBlock* block) {
  assert(block != nullptr);
  const std::int64_t total_lines = frame_count_ * ntsc::kLinesPerFrame;
  if (next_line_ >= total_lines) {
    return false;
  }
  const std::int64_t line_in_frame = next_line_ % ntsc::kLinesPerFrame;
  const std::int64_t lines =
      std::min(lines_per_block_, ntsc::kLinesPerFrame - line_in_frame);
  const std::int64_t offset =
      first_frame_ * kFrameBytes + next_line_ * kLineBytes;
  if (offset + lines * kLineBytes > source_->Size()) {
    throw std::runtime_error(
        "input frame " +
        std::to_string(first_frame_ + next_line_ / ntsc::kLinesPerFrame) +
        " is truncated");
  }

  std::vector<std::byte> bytes(static_cast<std::size_t>(lines * kLineBytes));
  source_->Read(offset, bytes);
  block->first_sample = next_line_ * ntsc::kSamplesPerLine;
  block->ire.assign(static_cast<std::size_t>(lines * ntsc::kSamplesPerLine),
                    0.0);
  DecodeSampleWords(encoding_, bytes, block->ire);
  next_line_ += lines;
  return true;
}

}  // namespace ld_creator
