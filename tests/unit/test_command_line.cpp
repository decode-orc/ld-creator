/*
 * File:        test_command_line.cpp
 * Module:      cli
 * Purpose:     Unit tests for command-line argument parsing
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "ld_creator/cli/command_line.h"

namespace ld_creator {
namespace {

CommandLineResult Parse(const std::vector<std::string_view>& arguments) {
  return ParseCommandLine(arguments);
}

// Parses arguments that are expected to be valid; records a test failure and
// returns default options if they are not.
CommandLineOptions ParseValid(const std::vector<std::string_view>& arguments) {
  const CommandLineResult result = Parse(arguments);
  if (!result.options.has_value()) {
    ADD_FAILURE() << "unexpected parse failure: " << result.error;
    return {};
  }
  return *result.options;
}

TEST(CommandLineTest, ReturnsDefaultsWhenGivenOnlyAProject) {
  const CommandLineOptions options = ParseValid({"--project", "p.yaml"});

  EXPECT_EQ(options.project_path, "p.yaml");
  EXPECT_FALSE(options.show_help);
  EXPECT_FALSE(options.show_version);
  EXPECT_FALSE(options.validate_only);
  EXPECT_FALSE(options.output_root.has_value());
  EXPECT_EQ(options.threads, 0);
  EXPECT_EQ(options.log_level, LogLevel::kInfo);
  EXPECT_TRUE(options.log_file.empty());
}

TEST(CommandLineTest, FailsWhenProjectIsMissing) {
  const CommandLineResult result = Parse({});

  EXPECT_FALSE(result.options.has_value());
  EXPECT_NE(result.error.find("--project is required"), std::string::npos);
  EXPECT_FALSE(Parse({"--validate"}).options.has_value());
  EXPECT_FALSE(Parse({"--project"}).options.has_value());
  EXPECT_FALSE(Parse({"--project", ""}).options.has_value());
}

TEST(CommandLineTest, DoesNotRequireProjectForHelpOrVersion) {
  EXPECT_TRUE(Parse({"--help"}).options.has_value());
  EXPECT_TRUE(Parse({"--version"}).options.has_value());
}

TEST(CommandLineTest, SetsValidateAndOutputRoot) {
  const CommandLineOptions options = ParseValid(
      {"--project", "p.yaml", "--validate", "--output-root", "build/out"});

  EXPECT_TRUE(options.validate_only);
  EXPECT_EQ(options.output_root, "build/out");
  EXPECT_FALSE(Parse({"--project", "p", "--output-root"}).options.has_value());
}

TEST(CommandLineTest, ParsesThreadCountOrAuto) {
  EXPECT_EQ(ParseValid({"--project", "p", "--threads", "auto"}).threads, 0);
  EXPECT_EQ(ParseValid({"--project", "p", "--threads", "1"}).threads, 1);
  EXPECT_EQ(ParseValid({"--project", "p", "--threads", "16"}).threads, 16);
  for (const char* bad : {"0", "-2", "x", "", "4097", "1.5"}) {
    EXPECT_FALSE(Parse({"--project", "p", "--threads", bad}).options) << bad;
  }
  EXPECT_FALSE(Parse({"--project", "p", "--threads"}).options.has_value());
}

TEST(CommandLineTest, SetsShowHelpWhenGivenShortOrLongHelpFlag) {
  EXPECT_TRUE(ParseValid({"--help"}).show_help);
  EXPECT_TRUE(ParseValid({"-h"}).show_help);
}

TEST(CommandLineTest, SetsShowVersionWhenGivenVersionFlag) {
  EXPECT_TRUE(ParseValid({"--version"}).show_version);
}

TEST(CommandLineTest, SetsLogLevelWhenGivenEachKnownLevel) {
  EXPECT_EQ(ParseValid({"--project", "p", "--log-level", "info"}).log_level,
            LogLevel::kInfo);
  EXPECT_EQ(ParseValid({"--project", "p", "--log-level", "debug"}).log_level,
            LogLevel::kDebug);
  EXPECT_EQ(ParseValid({"--project", "p", "--log-level", "trace"}).log_level,
            LogLevel::kTrace);
}

TEST(CommandLineTest, FailsWhenLogLevelIsUnknown) {
  const CommandLineResult result = Parse({"--log-level", "verbose"});

  EXPECT_FALSE(result.options.has_value());
  EXPECT_NE(result.error.find("verbose"), std::string::npos);
}

TEST(CommandLineTest, FailsWhenLogLevelValueIsMissing) {
  EXPECT_FALSE(Parse({"--log-level"}).options.has_value());
}

TEST(CommandLineTest, SetsLogFileWhenGivenPath) {
  EXPECT_EQ(
      ParseValid({"--project", "p", "--log-file", "out/run.log"}).log_file,
      "out/run.log");
}

TEST(CommandLineTest, FailsWhenLogFileIsMissingOrEmpty) {
  EXPECT_FALSE(Parse({"--log-file"}).options.has_value());
  EXPECT_FALSE(Parse({"--log-file", ""}).options.has_value());
}

TEST(CommandLineTest, FailsWhenGivenUnknownOption) {
  const CommandLineResult result = Parse({"--frobnicate"});

  EXPECT_FALSE(result.options.has_value());
  EXPECT_NE(result.error.find("--frobnicate"), std::string::npos);
}

TEST(CommandLineTest, UsageTextNamesProgramAndListsEveryOption) {
  const std::string usage = UsageText("ld-creator");

  EXPECT_NE(usage.find("Usage: ld-creator"), std::string::npos);
  for (const char* option :
       {"--project", "--validate", "--output-root", "--threads", "--help",
        "--version", "--log-level", "--log-file"}) {
    EXPECT_NE(usage.find(option), std::string::npos) << option;
  }
}

}  // namespace
}  // namespace ld_creator
