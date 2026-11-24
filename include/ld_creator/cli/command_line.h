/*
 * File:        command_line.h
 * Module:      cli
 * Purpose:     Parses ld-creator command-line arguments into typed options
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ld_creator {

// Console and file log verbosity (AGENTS.md §3.2).
enum class LogLevel { kInfo, kDebug, kTrace };

// Options selected on the command line. Defaults describe a plain invocation
// with no arguments.
struct CommandLineOptions {
  bool show_help = false;
  bool show_version = false;
  // --project: the project file. Required unless --help or --version is
  // given.
  std::string project_path;
  // --validate: check the project and its input, write nothing.
  bool validate_only = false;
  // --output-root: the directory {output} stands for. Unset means the
  // project file's directory.
  std::optional<std::string> output_root;
  // --threads: worker threads; 0 means one per hardware thread ("auto").
  // Never changes the output.
  int threads = 0;
  LogLevel log_level = LogLevel::kInfo;
  // Empty when logging goes to the console only.
  std::string log_file;
};

// Outcome of parsing: `options` is set on success, otherwise `error` holds a
// single-line diagnostic suitable for printing before the usage text.
struct CommandLineResult {
  std::optional<CommandLineOptions> options;
  std::string error;
};

// Parses `arguments`, which excludes the program name (argv[1..argc-1]).
// Pure function: no I/O and no global state, so it is safe to call from any
// thread.
CommandLineResult ParseCommandLine(std::span<const std::string_view> arguments);

// Returns the --help text for `program_name`.
std::string UsageText(std::string_view program_name);

}  // namespace ld_creator
