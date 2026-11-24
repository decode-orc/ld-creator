/*
 * File:        test_cli_executable.cpp
 * Module:      cli
 * Purpose:     Functional tests that run the built ld-creator executable
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>
#include <sys/wait.h>

#include <cstdlib>
#include <string>

#include "fixture_paths.h"

namespace ld_creator {
namespace {

// Runs the executable with `arguments`, discarding its output, and returns the
// process exit status.
int RunExecutable(const std::string& arguments) {
  const std::string command = std::string("\"") + test::kExecutablePath +
                              "\" " + arguments + " >/dev/null 2>&1";
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

TEST(CliExecutableTest, ExitsSuccessfullyWhenAskedForVersion) {
  EXPECT_EQ(RunExecutable("--version"), EXIT_SUCCESS);
}

TEST(CliExecutableTest, ExitsSuccessfullyWhenAskedForHelp) {
  EXPECT_EQ(RunExecutable("--help"), EXIT_SUCCESS);
}

TEST(CliExecutableTest, ExitsWithFailureWhenProjectIsMissing) {
  EXPECT_EQ(RunExecutable(""), EXIT_FAILURE);
  EXPECT_EQ(RunExecutable("--validate"), EXIT_FAILURE);
}

TEST(CliExecutableTest, ExitsWithFailureWhenProjectFileDoesNotExist) {
  EXPECT_EQ(RunExecutable("--project /nonexistent/project.yaml --validate"),
            EXIT_FAILURE);
}

TEST(CliExecutableTest, ExitsWithFailureWhenGivenUnknownOption) {
  EXPECT_EQ(RunExecutable("--frobnicate"), EXIT_FAILURE);
}

}  // namespace
}  // namespace ld_creator
