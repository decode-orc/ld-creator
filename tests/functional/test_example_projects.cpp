/*
 * File:        test_example_projects.cpp
 * Module:      cli
 * Purpose:     Functional tests: the executable validates the example projects
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>
#include <sqlite3.h>
#include <sys/wait.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "fixture_paths.h"

namespace ld_creator {
namespace {

namespace fs = std::filesystem;

// NTSC frame size in bytes (CVBS spec NTSC preset: 525 × 910 two-byte words).
constexpr std::int64_t kFrameBytes = 525LL * 910LL * 2LL;
// The example projects read 100 frames (all-keys.yaml sets frame_count: 100).
constexpr std::int64_t kExampleFrames = 100;

fs::path ExamplesDir() {
  return fs::path(test::kSourceDir) / "docs-tech" / "user" / "disc-model" /
         "examples";
}

// Writes a .meta with the CVBS spec schema (user_version 12) and one row.
void WriteMetadata(const fs::path& path, std::int64_t frames) {
  fs::remove(path);
  sqlite3* database = nullptr;
  ASSERT_EQ(sqlite3_open(path.string().c_str(), &database), SQLITE_OK);
  const std::string sql =
      "PRAGMA user_version = 12;"
      "CREATE TABLE cvbs_file (cvbs_file_id INTEGER PRIMARY KEY, preset TEXT "
      "NOT NULL, sample_encoding_preset TEXT NOT NULL, signal_state_preset "
      "TEXT NOT NULL, sequence_continuous BOOLEAN, signal_type TEXT NOT NULL, "
      "decoder TEXT NOT NULL, git_branch TEXT, git_commit TEXT, "
      "number_of_sequential_frames INTEGER, black_level INTEGER, "
      "has_nonstandard_values BOOLEAN, capture_notes TEXT);"
      "INSERT INTO cvbs_file (preset, sample_encoding_preset, "
      "signal_state_preset, signal_type, decoder, "
      "number_of_sequential_frames) VALUES ('NTSC', 'CVBS_S16_4FSC', "
      "'STANDARD_STABLE_LOCKED', 'composite', 'other', " +
      std::to_string(frames) + ");";
  char* error = nullptr;
  const int result =
      sqlite3_exec(database, sql.c_str(), nullptr, nullptr, &error);
  const std::string message = error == nullptr ? "" : error;
  sqlite3_free(error);
  sqlite3_close(database);
  ASSERT_EQ(result, SQLITE_OK) << message;
}

// Creates <dir>/ntsc_cav.cvbs (sparse, all zero words) and its .meta, as the
// example projects' {output}/ntsc_cav.cvbs.
void WriteInputMedia(const fs::path& dir, std::int64_t frames) {
  fs::create_directories(dir);
  const fs::path video = dir / "ntsc_cav.cvbs";
  {
    std::ofstream create(video, std::ios::binary | std::ios::trunc);
  }
  fs::resize_file(video, static_cast<std::uintmax_t>(frames * kFrameBytes));
  WriteMetadata(dir / "ntsc_cav.meta", frames);
}

int RunExecutable(const std::string& arguments) {
  const std::string command = std::string("\"") + test::kExecutablePath +
                              "\" " + arguments + " >/dev/null 2>&1";
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

class ExampleProjectsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    output_root_ = fs::path(test::kBinaryDir) / "functional" / "examples";
    WriteInputMedia(output_root_, kExampleFrames);
  }

  int Validate(const fs::path& project) {
    return RunExecutable("--project \"" + project.string() +
                         "\" --output-root \"" + output_root_.string() +
                         "\" --validate");
  }

  fs::path output_root_;
};

TEST_F(ExampleProjectsTest, ValidatesTheIdealCavExample) {
  EXPECT_EQ(Validate(ExamplesDir() / "ideal-ntsc-cav.yaml"), EXIT_SUCCESS);
}

TEST_F(ExampleProjectsTest, ValidatesTheAllKeysExample) {
  EXPECT_EQ(Validate(ExamplesDir() / "all-keys.yaml"), EXIT_SUCCESS);
}

TEST_F(ExampleProjectsTest, ValidationWritesNoOutput) {
  ASSERT_EQ(Validate(ExamplesDir() / "ideal-ntsc-cav.yaml"), EXIT_SUCCESS);

  EXPECT_FALSE(fs::exists(output_root_ / "ideal_ntsc_cav.s16"));
  EXPECT_FALSE(fs::exists(output_root_ / "ideal_ntsc_cav.resolved.yaml"));
}

TEST_F(ExampleProjectsTest, RejectsAProjectWhoseInputIsTooShort) {
  // all-keys.yaml reads 100 frames; give it 50.
  WriteInputMedia(output_root_, 50);

  EXPECT_EQ(Validate(ExamplesDir() / "all-keys.yaml"), EXIT_FAILURE);
}

TEST_F(ExampleProjectsTest, RejectsAProjectWithAnOutOfToleranceValue) {
  const fs::path project = output_root_ / "out_of_tolerance.yaml";
  {
    std::ofstream file(project);
    file << "input: {video_path: \"{output}/ntsc_cav.cvbs\"}\n"
            "disc_cutter: {format: cav, start_radius_mm: 55, "
            "track_pitch_um: 1.6667}\n"
            "video_pre_emphasis: {t1_ns: 340}\n"
            "output: {rf_path: \"{output}/x.s16\"}\n";
  }

  EXPECT_EQ(Validate(project), EXIT_FAILURE);
}

}  // namespace
}  // namespace ld_creator
