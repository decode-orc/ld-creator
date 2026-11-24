/*
 * File:        cvbs_metadata.cpp
 * Module:      cvbs_input
 * Purpose:     Reads and checks the input's CVBS .meta metadata
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/cvbs_input/cvbs_metadata.h"

#include <sqlite3.h>

#include <memory>

namespace ld_creator {

namespace {

struct DatabaseCloser {
  void operator()(sqlite3* database) const { sqlite3_close(database); }
};
struct StatementFinalizer {
  void operator()(sqlite3_stmt* statement) const {
    sqlite3_finalize(statement);
  }
};

std::string ColumnText(sqlite3_stmt* statement, int column) {
  const unsigned char* text = sqlite3_column_text(statement, column);
  return text == nullptr ? std::string()
                         : std::string(reinterpret_cast<const char*>(text));
}

CvbsMetadataResult Failure(const std::string& message) {
  return CvbsMetadataResult{.metadata = std::nullopt, .error = message};
}

}  // namespace

CvbsMetadataResult SqliteCvbsMetadataReader::Read(
    const std::string& meta_path) {
  sqlite3* raw_database = nullptr;
  const int opened = sqlite3_open_v2(meta_path.c_str(), &raw_database,
                                     SQLITE_OPEN_READONLY, nullptr);
  const std::unique_ptr<sqlite3, DatabaseCloser> database(raw_database);
  if (opened != SQLITE_OK) {
    return Failure("cannot open metadata " + meta_path);
  }

  constexpr const char* kQuery =
      "SELECT preset, sample_encoding_preset, signal_state_preset, "
      "signal_type, number_of_sequential_frames FROM cvbs_file";
  sqlite3_stmt* raw_statement = nullptr;
  if (sqlite3_prepare_v2(database.get(), kQuery, -1, &raw_statement, nullptr) !=
      SQLITE_OK) {
    return Failure(
        "metadata " + meta_path +
        " has no readable cvbs_file table: " + sqlite3_errmsg(database.get()));
  }
  const std::unique_ptr<sqlite3_stmt, StatementFinalizer> statement(
      raw_statement);

  if (sqlite3_step(statement.get()) != SQLITE_ROW) {
    return Failure("metadata " + meta_path + " has no cvbs_file row");
  }
  CvbsMetadata metadata;
  metadata.preset = ColumnText(statement.get(), 0);
  metadata.sample_encoding_preset = ColumnText(statement.get(), 1);
  metadata.signal_state_preset = ColumnText(statement.get(), 2);
  metadata.signal_type = ColumnText(statement.get(), 3);
  if (sqlite3_column_type(statement.get(), 4) != SQLITE_NULL) {
    metadata.number_of_sequential_frames =
        sqlite3_column_int64(statement.get(), 4);
  }
  if (sqlite3_step(statement.get()) == SQLITE_ROW) {
    return Failure("metadata " + meta_path +
                   " has more than one cvbs_file row");
  }
  return CvbsMetadataResult{.metadata = metadata, .error = {}};
}

std::string MetadataPathFor(const std::string& video_path) {
  const std::string suffix = ".cvbs";
  if (video_path.size() >= suffix.size() &&
      video_path.compare(video_path.size() - suffix.size(), suffix.size(),
                         suffix) == 0) {
    return video_path.substr(0, video_path.size() - suffix.size()) + ".meta";
  }
  return video_path + ".meta";
}

InputFormatResult CheckNtscCompositeInput(const CvbsMetadata& metadata) {
  const auto failure = [](const std::string& message) {
    return InputFormatResult{.encoding = std::nullopt, .error = message};
  };
  if (metadata.preset != "NTSC") {
    return failure("input video standard is '" + metadata.preset +
                   "'; only NTSC is supported");
  }
  if (metadata.signal_type != "composite") {
    return failure("input signal_type is '" + metadata.signal_type +
                   "'; a LaserVision disc records composite video");
  }
  // Time-base stable at the standard 4fsc rate: the stored-origin rule and
  // the 477 750-sample frame then hold (CVBS spec, signal-state presets).
  if (metadata.signal_state_preset != "STANDARD_STABLE_LOCKED" &&
      metadata.signal_state_preset != "STANDARD_STABLE_UNLOCKED") {
    return failure("input signal_state_preset is '" +
                   metadata.signal_state_preset +
                   "'; it must be STANDARD_STABLE_LOCKED or "
                   "STANDARD_STABLE_UNLOCKED (4fsc, time-base stable)");
  }
  const std::string& encoding = metadata.sample_encoding_preset;
  if (encoding == "CVBS_U10_4FSC") {
    return {.encoding = SampleEncoding::kU10, .error = {}};
  }
  if (encoding == "CVBS_U16_4FSC") {
    return {.encoding = SampleEncoding::kU16, .error = {}};
  }
  if (encoding == "CVBS_S16_4FSC") {
    return {.encoding = SampleEncoding::kS16, .error = {}};
  }
  if (encoding == "CVBS_TPG21_4FSC") {
    return {.encoding = SampleEncoding::kTpg21, .error = {}};
  }
  return failure("input sample_encoding_preset is '" + encoding +
                 "'; it must be CVBS_U10_4FSC, CVBS_U16_4FSC, CVBS_S16_4FSC "
                 "or CVBS_TPG21_4FSC");
}

}  // namespace ld_creator
