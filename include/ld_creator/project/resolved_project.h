/*
 * File:        resolved_project.h
 * Module:      project
 * Purpose:     Writes the resolved project with every effective value
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>
#include <string>

#include "ld_creator/project/project_config.h"

namespace ld_creator {

// Returns the resolved project as YAML text (docs/reference/project-file.md,
// "The resolved project"). `config` must be a loaded, validated project
// (absolute paths, hfmi filled); `frame_count` is the effective number of
// input frames; `record` is the resolved block to write.
//
// Every block and key is written, in the order of the reference page, with
// doubles in shortest round-trip form and strings double-quoted, so loading
// the text and resolving it again gives the same bytes. The output does not
// depend on locale, host or thread. Pure function.
std::string EmitResolvedProject(const ProjectConfig& config,
                                std::int64_t frame_count,
                                const ResolvedRecord& record);

// The resolved project's path for an RF path: ".s16" replaced by
// ".resolved.yaml".
std::string ResolvedProjectPath(const std::string& rf_path);

}  // namespace ld_creator
