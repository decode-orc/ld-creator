/*
 * File:        project_parser.h
 * Module:      project
 * Purpose:     Parses project YAML text into a typed configuration
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ld_creator/project/project_config.h"

namespace ld_creator {

// Errors and warnings gathered while loading a project. Each message is one
// line and names the key path it concerns.
struct ProjectDiagnostics {
  std::vector<std::string> errors;
  std::vector<std::string> warnings;

  [[nodiscard]] bool HasErrors() const { return !errors.empty(); }
};

struct ProjectParseResult {
  // Set when the text has the schema's structure and types. Ranges and
  // tolerances are checked separately (ValidateProject).
  std::optional<ProjectConfig> config;
  ProjectDiagnostics diagnostics;
};

// Parses project YAML (docs/reference/project-file.md). Strict: unknown or
// duplicate keys, wrong types and missing required keys are errors. Omitted
// keys take the defaults in ProjectConfig. Paths are returned as written.
//
// Pure function: no I/O, safe from any thread.
ProjectParseResult ParseProjectText(std::string_view text);

}  // namespace ld_creator
