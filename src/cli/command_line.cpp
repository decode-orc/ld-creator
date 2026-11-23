/*
 * File:        command_line.cpp
 * Module:      cli
 * Purpose:     Parses ld-creator command-line arguments into typed options
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/cli/command_line.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace ld_creator {

namespace {

std::optional<LogLevel> ParseLogLevel(std::string_view text) {
  if (text == "info") {
    return LogLevel::kInfo;
  }
  if (text == "debug") {
    return LogLevel::kDebug;
  }
  if (text == "trace") {
    return LogLevel::kTrace;
  }
  return std::nullopt;
}

CommandLineResult Failure(std::string message) {
  return CommandLineResult{.options = std::nullopt,
                           .error = std::move(message)};
}

}  // namespace

CommandLineResult ParseCommandLine(
    std::span<const std::string_view> arguments) {
  CommandLineOptions options;

  for (std::size_t index = 0; index < arguments.size(); ++index) {
    const std::string_view argument = arguments[index];
    const bool has_value = index + 1 < arguments.size();

    if (argument == "--help" || argument == "-h") {
      options.show_help = true;
    } else if (argument == "--version") {
      options.show_version = true;
    } else if (argument == "--log-level") {
      if (!has_value) {
        return Failure("--log-level requires a value (info, debug or trace)");
      }
      const std::string_view value = arguments[++index];
      const std::optional<LogLevel> level = ParseLogLevel(value);
      if (!level) {
        return Failure("unknown log level '" + std::string(value) +
                       "' (expected info, debug or trace)");
      }
      options.log_level = *level;
    } else if (argument == "--log-file") {
      if (!has_value) {
        return Failure("--log-file requires a path");
      }
      const std::string_view value = arguments[++index];
      if (value.empty()) {
        return Failure("--log-file requires a non-empty path");
      }
      options.log_file = std::string(value);
    } else {
      return Failure("unknown option '" + std::string(argument) + "'");
    }
  }

  return CommandLineResult{.options = options, .error = {}};
}

std::string UsageText(std::string_view program_name) {
  std::string text = "Usage: ";
  text += program_name;
  text +=
      " [options]\n"
      "\n"
      "Modulates CVBS video, analogue audio and EFM into a LaserDisc RF "
      "signal.\n"
      "\n"
      "Options:\n"
      "  -h, --help             Show this help and exit\n"
      "      --version          Print the build version and exit\n"
      "      --log-level <lvl>  info, debug or trace (default: info)\n"
      "      --log-file <path>  Also write log output to <path>\n";
  return text;
}

}  // namespace ld_creator
