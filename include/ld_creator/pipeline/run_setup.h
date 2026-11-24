/*
 * File:        run_setup.h
 * Module:      pipeline
 * Purpose:     Loads a project and checks it against its input media
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "ld_creator/cvbs_input/cvbs_metadata.h"
#include "ld_creator/cvbs_input/cvbs_video_input.h"
#include "ld_creator/project/project_config.h"
#include "ld_creator/project/project_loader.h"
#include "ld_creator/project/project_parser.h"

namespace ld_creator {

// Opens byte sources by path. Injected for unit tests.
class IByteSourceFactory {
 public:
  virtual ~IByteSourceFactory() = default;
  // Throws std::runtime_error if the file cannot be opened.
  virtual std::unique_ptr<IByteSource> Open(const std::string& path) = 0;
};

class FileByteSourceFactory final : public IByteSourceFactory {
 public:
  std::unique_ptr<IByteSource> Open(const std::string& path) override;
};

// SHA-256 of every byte of `source`, as 64 lower-case hex digits.
std::string HashByteSource(IByteSource* source);

// A project that has passed every check, with what the run needs to start.
struct PreparedRun {
  ProjectConfig config;
  std::string meta_path;
  SampleEncoding encoding = SampleEncoding::kU10;
  std::int64_t file_frame_count = 0;
  // Frames the run reads (EffectiveFrameCount).
  std::int64_t frame_count = 0;
};

struct RunSetupResult {
  std::optional<PreparedRun> run;
  ProjectDiagnostics diagnostics;
};

// The external services PrepareRun uses. All borrowed.
struct RunSetupServices {
  ITextFileReader* project_reader = nullptr;
  ICvbsMetadataReader* metadata_reader = nullptr;
  IByteSourceFactory* byte_sources = nullptr;
};

// Loads and validates the project, reads and checks the input's metadata,
// counts its frames, applies the media-dependent checks and, for a resolved
// project, checks the input hashes (a mismatch is an error naming the file)
// and the version (a mismatch is a warning).
RunSetupResult PrepareRun(const std::string& project_path,
                          const PathRoots& roots,
                          const std::string& ld_creator_version,
                          const RunSetupServices& services);

}  // namespace ld_creator
