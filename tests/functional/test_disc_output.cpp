/*
 * File:        test_disc_output.cpp
 * Module:      tests
 * Purpose:     Functional test of a disc run on videosynth-generated video
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>
#include <sys/wait.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "fixture_paths.h"

namespace ld_creator {
namespace {

namespace fs = std::filesystem;

// Three frames: n / 40 MHz < 3 × 1001/30000 s gives 4 004 000 samples.
constexpr std::uintmax_t kExpectedSamples = 4'004'000;

// DdD words of codes 0 and 1023 (S14): a clipped sample.
constexpr std::int16_t kLowestWord = -32768;
constexpr std::int16_t kHighestWord = 32704;

fs::path ProjectsDir() {
  return fs::path(test::kSourceDir) / "tests" / "functional" / "projects";
}

std::string ReadFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

int RunCommand(const std::string& command) {
  const int status = std::system((command + " >/dev/null 2>&1").c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

bool OnPath(const std::string& tool) {
  return RunCommand("command -v " + tool) == 0;
}

class DiscOutputTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (!OnPath("videosynth")) {
      GTEST_SKIP() << "videosynth is not on PATH (use the dev shell)";
    }
    // One directory per test, so ctest can run them in parallel.
    dir_ = fs::path(test::kBinaryDir) / "functional" / "disc_output" /
           ::testing::UnitTest::GetInstance()->current_test_info()->name();
    fs::remove_all(dir_);
    fs::create_directories(dir_);
    ASSERT_EQ(
        RunCommand("videosynth --project \"" +
                   (ProjectsDir() / "ntsc_bars_videosynth.yaml").string() +
                   "\" --output-root \"" + dir_.string() + "\""),
        EXIT_SUCCESS);
  }

  int RunDisc(const fs::path& project, int threads) {
    return RunCommand(std::string("\"") + test::kExecutablePath +
                      "\" --project \"" + project.string() +
                      "\" --output-root \"" + dir_.string() + "\" --threads " +
                      std::to_string(threads));
  }

  fs::path dir_;
};

// Plan Task 5.4 acceptance: a committed project on a committed videosynth
// project gives RF of the expected length; plan Task 5.3: without
// clipping, the level leaving 25 % of the window as headroom.
TEST_F(DiscOutputTest, WritesRfOfTheExpectedLengthWithoutClipping) {
  ASSERT_EQ(RunDisc(ProjectsDir() / "ntsc_bars_disc.yaml", 4), EXIT_SUCCESS);
  const std::string rf = ReadFile(dir_ / "ntsc_bars.s16");
  ASSERT_EQ(rf.size(), 2U * kExpectedSamples);
  std::int64_t clipped = 0;
  for (std::size_t i = 0; i < rf.size(); i += 2) {
    std::int16_t word = 0;
    std::memcpy(&word, rf.data() + i, sizeof(word));
    EXPECT_EQ(word % 64, 0);
    if (word == kLowestWord || word == kHighestWord) {
      ++clipped;
    }
  }
  EXPECT_EQ(clipped, 0);
  const std::string resolved = ReadFile(dir_ / "ntsc_bars.resolved.yaml");
  EXPECT_EQ(resolved.find("NOT A DISC"), std::string::npos);
  EXPECT_NE(resolved.find("rf_sha256"), std::string::npos);
}

// Plan Task 5.4 acceptance: byte-identical across thread counts, and
// re-running the resolved project reproduces the RF.
TEST_F(DiscOutputTest, RfIsTheSameForAnyThreadCountAndFromTheResolvedProject) {
  ASSERT_EQ(RunDisc(ProjectsDir() / "ntsc_bars_disc.yaml", 1), EXIT_SUCCESS);
  const std::string single = ReadFile(dir_ / "ntsc_bars.s16");
  const std::string resolved_text = ReadFile(dir_ / "ntsc_bars.resolved.yaml");

  ASSERT_EQ(RunDisc(ProjectsDir() / "ntsc_bars_disc.yaml", 6), EXIT_SUCCESS);
  EXPECT_EQ(ReadFile(dir_ / "ntsc_bars.s16"), single);

  fs::copy_file(dir_ / "ntsc_bars.resolved.yaml", dir_ / "rerun.yaml");
  ASSERT_EQ(RunDisc(dir_ / "rerun.yaml", 3), EXIT_SUCCESS);
  EXPECT_EQ(ReadFile(dir_ / "ntsc_bars.s16"), single);
  EXPECT_EQ(ReadFile(dir_ / "ntsc_bars.resolved.yaml"), resolved_text);
}

// Every tap of a disc run is written, and taps leave the RF unchanged.
TEST_F(DiscOutputTest, WritesEveryLaterStageTapWithoutChangingTheRf) {
  ASSERT_EQ(RunDisc(ProjectsDir() / "ntsc_bars_disc.yaml", 4), EXIT_SUCCESS);
  const std::string untapped = ReadFile(dir_ / "ntsc_bars.s16");
  std::string text = ReadFile(ProjectsDir() / "ntsc_bars_disc.yaml");
  text +=
      "\ndiagnostics:\n  taps:\n"
      "    - {stage: recording_limiter, path: \"{output}/d4\"}\n"
      "    - {stage: disc_cutter, path: \"{output}/d5\"}\n"
      "    - {stage: optical_readout, path: \"{output}/d8\"}\n"
      "    - {stage: player_front_end, path: \"{output}/d9\"}\n"
      "    - {stage: rf_capture, path: \"{output}/capture\", frame_count: 1}\n";
  std::ofstream(dir_ / "tapped.yaml") << text;
  ASSERT_EQ(RunDisc(dir_ / "tapped.yaml", 4), EXIT_SUCCESS);
  EXPECT_EQ(ReadFile(dir_ / "ntsc_bars.s16"), untapped);
  // One frame at 80 MHz: ⌈8 008 000/3⌉ = 2 669 334 samples.
  EXPECT_EQ(fs::file_size(dir_ / "d8.f64"), 8U * 2669334U);
  EXPECT_EQ(fs::file_size(dir_ / "d8.f64"), fs::file_size(dir_ / "d9.f64"));
  EXPECT_EQ(ReadFile(dir_ / "d8.f64"), ReadFile(dir_ / "d9.f64"));
  EXPECT_EQ(fs::file_size(dir_ / "capture.f64"), 8U * 2669334U);
  // Edges every ~62 ns: some 540 000 in a frame.
  EXPECT_GT(fs::file_size(dir_ / "d4.edges"), 9U * 500000U);
  EXPECT_EQ(fs::file_size(dir_ / "d4.edges") / 9,
            fs::file_size(dir_ / "d5.edges") / 17);
  for (const char* name : {"d4", "d5", "d8", "d9", "capture"}) {
    EXPECT_TRUE(fs::exists(dir_ / (std::string(name) + ".tap.yaml"))) << name;
  }
}

}  // namespace
}  // namespace ld_creator
