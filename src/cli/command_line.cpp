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

// "auto" (0) or a positive integer.
std::optional<int> ParseThreads(std::string_view text) {
  if (text == "auto") {
    return 0;
  }
  constexpr int kMaximumThreads = 4096;
  int value = 0;
  if (text.empty()) {
    return std::nullopt;
  }
  for (const char c : text) {
    if (c < '0' || c > '9') {
      return std::nullopt;
    }
    value = value * 10 + (c - '0');
    if (value > kMaximumThreads) {
      return std::nullopt;
    }
  }
  if (value < 1) {
    return std::nullopt;
  }
  return value;
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
    } else if (argument == "--project") {
      if (!has_value || arguments[index + 1].empty()) {
        return Failure("--project requires a path");
      }
      options.project_path = std::string(arguments[++index]);
    } else if (argument == "--validate") {
      options.validate_only = true;
    } else if (argument == "--output-root") {
      if (!has_value || arguments[index + 1].empty()) {
        return Failure("--output-root requires a directory");
      }
      options.output_root = std::string(arguments[++index]);
    } else if (argument == "--threads") {
      if (!has_value) {
        return Failure("--threads requires a value (auto or a count)");
      }
      const std::string_view value = arguments[++index];
      const std::optional<int> threads = ParseThreads(value);
      if (!threads) {
        return Failure("invalid thread count '" + std::string(value) +
                       "' (expected auto or 1–4096)");
      }
      options.threads = *threads;
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

  if (options.project_path.empty() && !options.show_help &&
      !options.show_version) {
    return Failure("--project is required");
  }
  return CommandLineResult{.options = options, .error = {}};
}

std::string UsageText(std::string_view program_name) {
  std::string text = "Usage: ";
  text += program_name;
  text +=
      " --project <path> [options]\n"
      "\n"
      "Modulates CVBS video, analogue audio and EFM into a LaserDisc RF "
      "signal.\n"
      "The project file describes the whole run; no option changes the "
      "output.\n"
      "\n"
      "Options:\n"
      "      --project <path>     Project YAML file (required)\n"
      "      --validate           Check the project and its input, write "
      "nothing\n"
      "      --output-root <dir>  Directory that {output} stands for "
      "(default:\n"
      "                           the project file's directory)\n"
      "      --threads <n|auto>   Worker threads (default: auto)\n"
      "      --log-level <lvl>    info, debug or trace (default: info)\n"
      "      --log-file <path>    Also write log output to <path>\n"
      "  -h, --help               Show this help and exit\n"
      "      --version            Print the build version and exit\n";
  return text;
}

}  // namespace ld_creator
