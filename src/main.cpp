/*
 * File:        main.cpp
 * Module:      cli
 * Purpose:     ld-creator command-line entry point
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

#include "ld_creator/cli/command_line.h"
#include "ld_creator_version.h"

namespace {

spdlog::level::level_enum ToSpdlogLevel(ld_creator::LogLevel level) {
  switch (level) {
    case ld_creator::LogLevel::kDebug:
      return spdlog::level::debug;
    case ld_creator::LogLevel::kTrace:
      return spdlog::level::trace;
    case ld_creator::LogLevel::kInfo:
      break;
  }
  return spdlog::level::info;
}

void ConfigureLogging(const ld_creator::CommandLineOptions& options) {
  std::vector<spdlog::sink_ptr> sinks;
  sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
  if (!options.log_file.empty()) {
    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        options.log_file, /*truncate=*/true));
  }
  auto logger = std::make_shared<spdlog::logger>("ld-creator", sinks.begin(),
                                                 sinks.end());
  logger->set_level(ToSpdlogLevel(options.log_level));
  spdlog::set_default_logger(logger);
}

}  // namespace

int main(int argc, char* argv[]) {
  const std::string_view program_name = argc > 0 ? argv[0] : "ld-creator";
  const std::vector<std::string_view> arguments(argv + (argc > 0 ? 1 : 0),
                                                argv + argc);

  const ld_creator::CommandLineResult parsed =
      ld_creator::ParseCommandLine(arguments);
  if (!parsed.options) {
    std::cerr << "ld-creator: " << parsed.error << "\n\n"
              << ld_creator::UsageText(program_name);
    return EXIT_FAILURE;
  }
  const ld_creator::CommandLineOptions& options = *parsed.options;

  if (options.show_help) {
    std::cout << ld_creator::UsageText(program_name);
    return EXIT_SUCCESS;
  }
  if (options.show_version) {
    std::cout << "ld-creator " << LD_CREATOR_VERSION << "\n";
    return EXIT_SUCCESS;
  }

  try {
    ConfigureLogging(options);
  } catch (const std::exception& error) {
    std::cerr << "ld-creator: cannot open log output: " << error.what() << "\n";
    return EXIT_FAILURE;
  }

  spdlog::info("ld-creator {}", LD_CREATOR_VERSION);
  // The modulation pipeline is designed but not yet implemented; see
  // docs-tech/design/high-level-design.md.
  spdlog::error("RF modulation is not implemented yet");
  return EXIT_FAILURE;
}
