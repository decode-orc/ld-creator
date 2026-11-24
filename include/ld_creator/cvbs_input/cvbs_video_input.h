/*
 * File:        cvbs_video_input.h
 * Module:      cvbs_input
 * Purpose:     S1: reads NTSC .cvbs samples as IRE (D1)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "ld_creator/cvbs_input/cvbs_metadata.h"
#include "ld_creator/pipeline/stage_interfaces.h"

namespace ld_creator {

// SMPTE 244M-2003 4.2.1: NTSC blanking is 10-bit code 240 (register
// S1.blanking_code).
inline constexpr double kNtscBlankingCode = 240.0;
// SMPTE 244M-2003 4.2.1: NTSC 100 % white is 10-bit code 800 (register
// S1.white_code).
inline constexpr double kNtscWhiteCode = 800.0;

// Bytes per stored sample word in every 4fsc encoding (CVBS spec).
inline constexpr std::int64_t kBytesPerSampleWord = 2;

// Decodes little-endian stored words to IRE (S1 note, "Decoding to the
// 10-bit domain" and "Conversion to IRE"):
//   v = w (U10), w/64 (U16), w/32 + blanking (S16), w/64 + 508 (TPG21);
//   IRE = 100·(v − blanking)/(white − blanking).
// Values outside 0–1023 are carried through, not clamped. `bytes` holds
// 2·ire.size() bytes. Pure function. O(n).
void DecodeSampleWords(SampleEncoding encoding,
                       std::span<const std::byte> bytes, std::span<double> ire);

// Random access to the bytes of the .cvbs file. Injected for unit tests.
class IByteSource {
 public:
  virtual ~IByteSource() = default;
  [[nodiscard]] virtual std::int64_t Size() const = 0;
  // Reads exactly bytes.size() bytes at `offset`. Throws std::runtime_error
  // on a short read.
  virtual void Read(std::int64_t offset, std::span<std::byte> bytes) = 0;
};

// A file opened for reading.
class FileByteSource final : public IByteSource {
 public:
  // Throws std::runtime_error if the file cannot be opened.
  explicit FileByteSource(const std::string& path);
  [[nodiscard]] std::int64_t Size() const override;
  void Read(std::int64_t offset, std::span<std::byte> bytes) override;

 private:
  std::string path_;
  std::ifstream stream_;
  std::int64_t size_ = 0;
};

struct FrameCountResult {
  std::optional<std::int64_t> frames;
  std::string error;
};

// The number of whole frames in a file of `file_bytes` bytes. A partial final
// frame, or a count that disagrees with the metadata's
// number_of_sequential_frames, is an error naming the frame.
FrameCountResult CountInputFrames(const CvbsMetadata& metadata,
                                  std::int64_t file_bytes);

// S1 cvbs_input. Reads `frame_count` frames from `first_frame` and produces
// D1: IRE at 4fsc, sample n at t = (n + 57/90)/4fsc, with t = 0 at 0H of
// frame line 1 of the first frame read. Each block holds `lines_per_block`
// lines (fewer at the end of a frame), so blocks never cross a frame
// boundary.
//
// Thread safety: one thread at a time. Ownership: borrows `source`.
class CvbsVideoInput final : public ICvbsInput {
 public:
  CvbsVideoInput(IByteSource* source, SampleEncoding encoding,
                 std::int64_t first_frame, std::int64_t frame_count,
                 std::int64_t lines_per_block);

  [[nodiscard]] VideoInputStream Stream() const override;
  bool ReadBlock(IreSampleBlock* block) override;

 private:
  IByteSource* source_;
  SampleEncoding encoding_;
  std::int64_t first_frame_;
  std::int64_t frame_count_;
  std::int64_t lines_per_block_;
  // Next line to read, counted from line 0 of the first frame read.
  std::int64_t next_line_ = 0;
};

}  // namespace ld_creator
