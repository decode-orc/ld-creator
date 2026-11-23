/*
 * File:        fixture_paths.h
 * Module:      tests
 * Purpose:     Build-time paths shared by the test suites
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

namespace ld_creator::test {

// Repository root, for locating committed fixtures. Read-only: tests must
// never write into the source tree (TESTING.md, "Repository layout").
inline constexpr const char* kSourceDir = LD_CREATOR_SOURCE_DIR;

// Build root. Functional tests create scratch output beneath it.
inline constexpr const char* kBinaryDir = LD_CREATOR_BINARY_DIR;

#ifdef LD_CREATOR_EXECUTABLE
// The built ld-creator executable (functional suites only).
inline constexpr const char* kExecutablePath = LD_CREATOR_EXECUTABLE;
#endif

}  // namespace ld_creator::test
