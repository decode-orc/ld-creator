/*
 * File:        project_loader.h
 * Module:      project
 * Purpose:     Reads, parses, resolves and validates a project file
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <optional>
#include <string>

#include "ld_creator/project/project_config.h"
#include "ld_creator/project/project_parser.h"

namespace ld_creator {

// Reads whole text files. Injected so the loader can be tested on text held
// in memory.
class ITextFileReader {
 public:
  virtual ~ITextFileReader() = default;
  // Returns the file's contents, or nullopt if it cannot be read.
  virtual std::optional<std::string> ReadText(const std::string& path) = 0;
};

// Reads files from the filesystem.
class FileTextReader final : public ITextFileReader {
 public:
  std::optional<std::string> ReadText(const std::string& path) override;
};

// The directories that the {project} and {output} path roots stand for.
struct PathRoots {
  // Absolute directory of the project file.
  std::string project_dir;
  // --output-root; unset means the project directory.
  std::optional<std::string> output_root;
};

// Resolves a project path to an absolute, lexically normal path:
//   "{project}/x" → project_dir/x;
//   "{output}/x"  → output_root/x (or project_dir/x without --output-root);
//   a relative path is relative to project_dir; an absolute path is kept.
// Returns nullopt for any other "{name}" root.
std::optional<std::string> ResolveProjectPath(const std::string& path,
                                              const PathRoots& roots);

struct ProjectLoadResult {
  // Set when the project parsed and passed ValidateProject. Paths are
  // absolute and optical_readout.hfmi is filled.
  std::optional<ProjectConfig> config;
  ProjectDiagnostics diagnostics;
};

// Loads a project: read, parse, resolve paths, validate. Media-dependent
// checks (ValidateAgainstInput) are left to the caller, which opens the
// input.
//
// Thread safety: as the injected reader. Ownership: borrows `reader`.
class ProjectLoader {
 public:
  explicit ProjectLoader(ITextFileReader* reader);

  // `project_path` is used as given for reading; `project_dir` in `roots`
  // must be its absolute directory.
  ProjectLoadResult Load(const std::string& project_path,
                         const PathRoots& roots);

 private:
  ITextFileReader* reader_;
};

}  // namespace ld_creator
