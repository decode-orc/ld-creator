/*
 * File:        project_validator.h
 * Module:      project
 * Purpose:     Range, IEC tolerance and cross-key project checks
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ld_creator/project/project_config.h"
#include "ld_creator/project/project_parser.h"

namespace ld_creator {

// Key paths that have an IEC tolerance, and so may be listed in
// allow_out_of_tolerance.
const std::vector<std::string>& ToleranceKeyPaths();

// Checks everything that does not depend on the input media: ranges, IEC
// tolerances (honouring allow_out_of_tolerance), cross-key rules and the
// allow list itself. Fills computed defaults: optical_readout.hfmi.
//
// Pure function: safe from any thread.
void ValidateProject(ProjectConfig* config, ProjectDiagnostics* diagnostics);

// Number of frames a run reads: input.frame_count, or every frame of the file
// from input.first_frame.
std::int64_t EffectiveFrameCount(const InputConfig& input,
                                 std::int64_t file_frame_count);

// Radius in mm at the end of `frame_count` frames (S8 note):
//   CAV: r = r0 + p·N (one revolution per frame, IEC 60857-1986 4.7.1);
//   CLV: r = √(r0² + p·v·t/π), t = N / frame rate (IEC 60857-1986 4.7.2).
double EndRadiusMm(const DiscCutterConfig& cutter, std::int64_t frame_count);

// Checks that depend on the input file's frame count: the input frame range,
// the tap frame ranges, and the §4.12 tolerance on the radius at the end of
// the run.
void ValidateAgainstInput(const ProjectConfig& config,
                          std::int64_t file_frame_count,
                          ProjectDiagnostics* diagnostics);

}  // namespace ld_creator
