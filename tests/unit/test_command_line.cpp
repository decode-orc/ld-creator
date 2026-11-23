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

TEST(CommandLineTest, ReturnsDefaultsWhenGivenNoArguments) {
  const CommandLineOptions options = ParseValid({});

  EXPECT_FALSE(options.show_help);
  EXPECT_FALSE(options.show_version);
  EXPECT_EQ(options.log_level, LogLevel::kInfo);
  EXPECT_TRUE(options.log_file.empty());
}

TEST(CommandLineTest, SetsShowHelpWhenGivenShortOrLongHelpFlag) {
  EXPECT_TRUE(ParseValid({"--help"}).show_help);
  EXPECT_TRUE(ParseValid({"-h"}).show_help);
}

TEST(CommandLineTest, SetsShowVersionWhenGivenVersionFlag) {
  EXPECT_TRUE(ParseValid({"--version"}).show_version);
}

TEST(CommandLineTest, SetsLogLevelWhenGivenEachKnownLevel) {
  EXPECT_EQ(ParseValid({"--log-level", "info"}).log_level, LogLevel::kInfo);
  EXPECT_EQ(ParseValid({"--log-level", "debug"}).log_level, LogLevel::kDebug);
  EXPECT_EQ(ParseValid({"--log-level", "trace"}).log_level, LogLevel::kTrace);
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
  EXPECT_EQ(ParseValid({"--log-file", "out/run.log"}).log_file, "out/run.log");
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
       {"--help", "--version", "--log-level", "--log-file"}) {
    EXPECT_NE(usage.find(option), std::string::npos) << option;
  }
}

}  // namespace
}  // namespace ld_creator
