/*
 * File:        cvbs_metadata.h
 * Module:      cvbs_input
 * Purpose:     Reads and checks the input's CVBS .meta metadata
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ld_creator {

// The 4fsc sample encodings S1 decodes (CVBS spec, sample-encoding presets).
enum class SampleEncoding {
  kU10,    // CVBS_U10_4FSC
  kU16,    // CVBS_U16_4FSC
  kS16,    // CVBS_S16_4FSC
  kTpg21,  // CVBS_TPG21_4FSC
};

// The `cvbs_file` row of a .meta database (CVBS spec, "Metadata Schema"),
// as text, before any check.
struct CvbsMetadata {
  std::string preset;
  std::string sample_encoding_preset;
  std::string signal_state_preset;
  std::string signal_type;
  std::optional<std::int64_t> number_of_sequential_frames;
};

struct CvbsMetadataResult {
  std::optional<CvbsMetadata> metadata;
  std::string error;
};

// Reads the metadata of a recording. Injected so S1 can be tested without a
// database.
class ICvbsMetadataReader {
 public:
  virtual ~ICvbsMetadataReader() = default;
  virtual CvbsMetadataResult Read(const std::string& meta_path) = 0;
};

// Reads a .meta SQLite database, read-only.
class SqliteCvbsMetadataReader final : public ICvbsMetadataReader {
 public:
  CvbsMetadataResult Read(const std::string& meta_path) override;
};

// The .meta path for a .cvbs path: the extension replaced, as videosynth and
// the CVBS spec name it (<base>.cvbs → <base>.meta).
std::string MetadataPathFor(const std::string& video_path);

struct InputFormatResult {
  std::optional<SampleEncoding> encoding;
  std::string error;
};

// S1 note: the input must be NTSC composite, sampled at 4fsc and time-base
// stable, in one of the four 4fsc encodings. Returns the encoding, or an
// error naming the offending field.
InputFormatResult CheckNtscCompositeInput(const CvbsMetadata& metadata);

}  // namespace ld_creator
