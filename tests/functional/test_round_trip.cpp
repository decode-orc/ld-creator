/*
 * File:        test_round_trip.cpp
 * Module:      tests
 * Purpose:     Functional test of the NTSC round trip through ld-decode
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>
#include <sys/wait.h>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "fixture_paths.h"

namespace ld_creator {
namespace {

namespace fs = std::filesystem;

// scripts/round_trip.py exit status for a tool failure or a failed
// comparator check, as against a confirmed (0) or refuted (1) prediction.
constexpr int kHarnessFailure = 2;

std::string ReadFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

int RunCommand(const std::string& command) {
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

bool Available(const std::string& command) {
  return RunCommand(command + " >/dev/null 2>&1") == 0;
}

// The verdict on one measurement's row of results.md, or "" if absent.
std::string Verdict(const std::string& results, const std::string& key) {
  const std::size_t row = results.find("\n| " + key + " |");
  if (row == std::string::npos) {
    return "";
  }
  const std::size_t end = results.find('\n', row + 1);
  const std::string line = results.substr(row + 1, end - row - 1);
  const std::size_t last = line.rfind(" |");
  const std::size_t previous = line.rfind("| ", last);
  return line.substr(previous + 2, last - previous - 2);
}

// Plan Task 7.3: the round trip on the carrier diagnostic, skipped when a
// sister tool or the analysis Python is missing. -B keeps Python's bytecode
// cache out of the source tree; -E keeps the ld-decode package's PYTHONPATH
// out of the analysis interpreter (scripts/README.md).
//
// The levels and VBI codes must be recovered. The chroma and pulse
// measurements are refuted by discrepancies still under triage
// (docs-tech/user/ld-decode-findings.md), so they are not asserted here.
TEST(RoundTripTest, CarrierDiagnosticRoundTripRecoversLevelsAndCodes) {
  for (const char* tool : {"videosynth", "ld-decode", "python3"}) {
    if (!Available(std::string("command -v ") + tool)) {
      GTEST_SKIP() << tool << " is not on PATH (use the dev shell)";
    }
  }
  if (!Available("python3 -E -B -c 'import numpy, scipy'")) {
    GTEST_SKIP()
        << "the analysis Python has no NumPy/SciPy (use the dev shell)";
  }
  const fs::path out = fs::path(test::kBinaryDir) / "functional" / "round_trip";
  fs::remove_all(out);
  fs::create_directories(out.parent_path());
  const fs::path script =
      fs::path(test::kSourceDir) / "scripts" / "round_trip.py";
  const fs::path build = fs::path(test::kExecutablePath).parent_path();
  const int status = RunCommand(
      "python3 -E -B \"" + script.string() + "\" --build-dir \"" +
      build.string() + "\" --output-dir \"" + out.string() +
      "\" --baselines carrier >\"" + (out.string() + ".log") + "\" 2>&1");
  ASSERT_NE(status, kHarnessFailure) << ReadFile(out.string() + ".log");
  const std::string results = ReadFile(out / "results.md");
  EXPECT_NE(results.find("Truth against itself: **confirmed**"),
            std::string::npos);
  EXPECT_NE(results.find("Truth against truth + 2 IRE: **refuted**"),
            std::string::npos);
  for (const char* key :
       {"L1", "L2", "L3b", "L3l", "L4", "L5", "V1", "V2", "V3"}) {
    EXPECT_EQ(Verdict(results, key), "confirmed") << key << "\n" << results;
  }
  // The reference IEC 60857 demodulator decodes the carrier diagnostic to
  // the truth: ld-creator's FM signal is right by the standard.
  const std::size_t reference =
      results.find("Carrier diagnostic: reference demodulator");
  ASSERT_NE(reference, std::string::npos) << results;
  EXPECT_NE(results.find("Verdict: **confirmed**", reference),
            std::string::npos)
      << results.substr(reference);
  // Plan Task 7.1 acceptance: every round-trip project validates against the
  // generated video.
  for (const char* name :
       {"carrier", "cav_inner", "cav_outer", "clv", "cav_inner_equalised"}) {
    const fs::path project = fs::path(test::kSourceDir) / "tests" /
                             "functional" / "projects" /
                             ("ntsc_round_trip_" + std::string(name) + ".yaml");
    EXPECT_EQ(RunCommand(std::string("\"") + test::kExecutablePath +
                         "\" --project \"" + project.string() +
                         "\" --output-root \"" + out.string() +
                         "\" --validate >/dev/null 2>&1"),
              EXIT_SUCCESS)
        << name;
  }
}

}  // namespace
}  // namespace ld_creator
