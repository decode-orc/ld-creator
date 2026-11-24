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

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "ld_creator/cli/command_line.h"
#include "ld_creator/diagnostics/byte_sink.h"
#include "ld_creator/pipeline/carrier_diagnostic_run.h"
#include "ld_creator/pipeline/disc_run.h"
#include "ld_creator/pipeline/run_setup.h"
#include "ld_creator/project/project_validator.h"
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

// Loads and checks the project and its input; with --validate, stops there.
int RunProject(const ld_creator::CommandLineOptions& options) {
  namespace fs = std::filesystem;
  std::error_code error;
  const fs::path project_file = fs::absolute(options.project_path, error);
  if (error) {
    spdlog::error("cannot resolve project path {}", options.project_path);
    return EXIT_FAILURE;
  }
  const ld_creator::PathRoots roots{
      .project_dir = project_file.parent_path().lexically_normal().string(),
      .output_root = options.output_root};
  spdlog::debug("Project file: {}", project_file.string());
  spdlog::debug("{{project}} = {}", roots.project_dir);
  spdlog::debug("{{output}} = {}",
                options.output_root ? *options.output_root : roots.project_dir);
  const unsigned int threads =
      options.threads > 0 ? static_cast<unsigned int>(options.threads)
                          : std::max(1U, std::thread::hardware_concurrency());
  spdlog::debug("Worker threads: {}", threads);

  ld_creator::FileTextReader project_reader;
  ld_creator::SqliteCvbsMetadataReader metadata_reader;
  ld_creator::FileByteSourceFactory byte_sources;
  const ld_creator::RunSetupResult setup =
      ld_creator::PrepareRun(options.project_path, roots, LD_CREATOR_VERSION,
                             {.project_reader = &project_reader,
                              .metadata_reader = &metadata_reader,
                              .byte_sources = &byte_sources});
  for (const std::string& warning : setup.diagnostics.warnings) {
    spdlog::warn("{}", warning);
  }
  for (const std::string& message : setup.diagnostics.errors) {
    spdlog::error("{}", message);
  }
  if (!setup.run) {
    return EXIT_FAILURE;
  }

  const ld_creator::PreparedRun& run = *setup.run;
  const ld_creator::DiscCutterConfig& cutter = run.config.disc_cutter;
  spdlog::info("Input: {} ({} of {} frames from frame {})",
               run.config.input.video_path, run.frame_count,
               run.file_frame_count, run.config.input.first_frame);
  spdlog::info("Disc: {}, {:.4f} mm to {:.4f} mm at {} um pitch",
               ld_creator::ToString(cutter.format), cutter.start_radius_mm,
               ld_creator::EndRadiusMm(cutter, run.frame_count),
               cutter.track_pitch_um);
  spdlog::debug("Resolved hfmi: {}",
                run.config.optical_readout.hfmi.value_or(0.0));
  spdlog::info("Output: {}", run.config.output.rf_path);

  if (options.validate_only) {
    spdlog::info("Project is valid");
    return EXIT_SUCCESS;
  }
  const bool carrier_diagnostic =
      run.config.output.mode == ld_creator::OutputMode::kCarrierDiagnostic;
  if (carrier_diagnostic) {
    spdlog::warn("output.mode = carrier_diagnostic: the RF is NOT A DISC");
  } else {
    spdlog::info("Capture: {}, level {} of full scale",
                 ld_creator::ToString(run.config.rf_capture.profile),
                 run.config.rf_capture.level_fraction);
  }
  ld_creator::FileByteSinkFactory byte_sinks;
  const ld_creator::RunOutputServices services{.byte_sources = &byte_sources,
                                               .byte_sinks = &byte_sinks};
  ld_creator::RfRunResult result;
  try {
    result =
        carrier_diagnostic
            ? ld_creator::RunCarrierDiagnostic(run, LD_CREATOR_VERSION, threads,
                                               services)
            : ld_creator::RunDisc(run, LD_CREATOR_VERSION, threads, services);
  } catch (const std::exception& error) {
    spdlog::error("Run failed: {}", error.what());
    return EXIT_FAILURE;
  }
  spdlog::info("Wrote {} RF samples at 40 MHz", result.rf_samples);
  if (result.clipped_samples > 0) {
    spdlog::warn("{} RF samples clipped at the converter's range",
                 result.clipped_samples);
  }
  spdlog::debug("RF SHA-256: {}", result.rf_sha256);
  spdlog::info("Resolved project: {}", result.resolved_path);
  return EXIT_SUCCESS;
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
  return RunProject(options);
}
