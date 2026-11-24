/*
 * File:        project_loader.cpp
 * Module:      project
 * Purpose:     Reads, parses, resolves and validates a project file
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/project/project_loader.h"

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <utility>

#include "ld_creator/project/project_validator.h"

namespace ld_creator {

std::optional<std::string> FileTextReader::ReadText(const std::string& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return std::nullopt;
  }
  std::ostringstream text;
  text << stream.rdbuf();
  if (stream.bad()) {
    return std::nullopt;
  }
  return text.str();
}

std::optional<std::string> ResolveProjectPath(const std::string& path,
                                              const PathRoots& roots) {
  namespace fs = std::filesystem;
  const fs::path project_dir(roots.project_dir);
  fs::path base = project_dir;
  std::string rest = path;
  if (!path.empty() && path.front() == '{') {
    const std::size_t close = path.find('}');
    if (close == std::string::npos) {
      return std::nullopt;
    }
    const std::string name = path.substr(1, close - 1);
    rest = path.substr(close + 1);
    if (!rest.empty() && (rest.front() == '/' || rest.front() == '\\')) {
      rest.erase(0, 1);
    }
    if (name == "output" && roots.output_root) {
      // A relative --output-root is relative to the project directory, as
      // in videosynth.
      base = (project_dir / fs::path(*roots.output_root)).lexically_normal();
    } else if (name != "project" && name != "output") {
      return std::nullopt;
    }
  } else if (fs::path(path).is_absolute()) {
    return fs::path(path).lexically_normal().string();
  }
  return (base / fs::path(rest)).lexically_normal().string();
}

ProjectLoader::ProjectLoader(ITextFileReader* reader) : reader_(reader) {
  assert(reader_ != nullptr);
}

ProjectLoadResult ProjectLoader::Load(const std::string& project_path,
                                      const PathRoots& roots) {
  ProjectLoadResult result;
  const std::optional<std::string> text = reader_->ReadText(project_path);
  if (!text) {
    result.diagnostics.errors.push_back("Cannot read project file: " +
                                        project_path);
    return result;
  }

  ProjectParseResult parsed = ParseProjectText(*text);
  result.diagnostics = std::move(parsed.diagnostics);
  if (!parsed.config) {
    return result;
  }
  ProjectConfig config = std::move(*parsed.config);

  const auto resolve = [&](const std::string& key_path, std::string* value) {
    if (value->empty()) {
      return;
    }
    if (const std::optional<std::string> resolved =
            ResolveProjectPath(*value, roots)) {
      *value = *resolved;
    } else {
      result.diagnostics.errors.push_back(
          "Project validation error: " + key_path + " = '" + *value +
          "' uses an unknown path root (only {project} and {output} exist)");
    }
  };
  resolve("input.video_path", &config.input.video_path);
  resolve("output.rf_path", &config.output.rf_path);
  for (std::size_t index = 0; index < config.taps.size(); ++index) {
    resolve("diagnostics.taps[" + std::to_string(index) + "].path",
            &config.taps[index].path);
  }

  ValidateProject(&config, &result.diagnostics);
  if (!result.diagnostics.HasErrors()) {
    result.config = std::move(config);
  }
  return result;
}

}  // namespace ld_creator
