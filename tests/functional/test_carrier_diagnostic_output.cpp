/*
 * File:        test_carrier_diagnostic_output.cpp
 * Module:      tests
 * Purpose:     Functional test of a carrier-diagnostic run of the executable
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
#include <iterator>
#include <string>
#include <vector>

#include "fixture_paths.h"

namespace ld_creator {
namespace {

namespace fs = std::filesystem;

constexpr std::int64_t kFrameWords = 525LL * 910LL;
constexpr std::int64_t kFrames = 2;

void WriteMetadata(const fs::path& path) {
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
      "number_of_sequential_frames) VALUES ('NTSC', 'CVBS_U10_4FSC', "
      "'STANDARD_STABLE_LOCKED', 'composite', 'other', " +
      std::to_string(kFrames) + ");";
  char* error = nullptr;
  const int result =
      sqlite3_exec(database, sql.c_str(), nullptr, nullptr, &error);
  const std::string message = error == nullptr ? "" : error;
  sqlite3_free(error);
  sqlite3_close(database);
  ASSERT_EQ(result, SQLITE_OK) << message;
}

// Two frames of U10 words: 67 samples of sync tip (16) at the start of each
// line, then a ramp from black (282) to white (800) across the line.
void WriteVideo(const fs::path& path) {
  std::vector<char> bytes;
  for (std::int64_t i = 0; i < kFrames * kFrameWords; ++i) {
    const std::int64_t sample = i % 910;
    const std::int64_t code =
        sample < 67 ? 16 : 282 + (((sample - 67) * (800 - 282)) / (910 - 67));
    bytes.push_back(static_cast<char>(code & 0xFF));
    bytes.push_back(static_cast<char>(code >> 8));
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

std::string ReadFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

int RunExecutable(const std::string& arguments) {
  const std::string command = std::string("\"") + test::kExecutablePath +
                              "\" " + arguments + " >/dev/null 2>&1";
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

class CarrierDiagnosticOutputTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // One directory per test, so ctest can run them in parallel.
    dir_ = fs::path(test::kBinaryDir) / "functional" / "carrier_diagnostic" /
           ::testing::UnitTest::GetInstance()->current_test_info()->name();
    fs::remove_all(dir_);
    fs::create_directories(dir_);
    WriteVideo(dir_ / "ramp.cvbs");
    WriteMetadata(dir_ / "ramp.meta");
    std::ofstream(dir_ / "carrier.yaml")
        << "input:\n  video_path: \"{project}/ramp.cvbs\"\n"
           "disc_cutter:\n  format: cav\n  start_radius_mm: 55.0\n"
           "  track_pitch_um: 1.6667\n"
           "output:\n  rf_path: \"{project}/carrier.s16\"\n"
           "  mode: carrier_diagnostic\n"
           "diagnostics:\n  taps:\n"
           "    - {stage: fm_phase_modulator, path: \"{project}/phase\"}\n";
  }

  fs::path dir_;
};

// Plan Task 3.5 acceptance.
TEST_F(CarrierDiagnosticOutputTest,
       WritesRfOfTheExpectedLengthFlaggedNotADisc) {
  ASSERT_EQ(RunExecutable("--project \"" + (dir_ / "carrier.yaml").string() +
                          "\" --threads 4"),
            EXIT_SUCCESS);

  // 40 MHz × 2 × 1001/30000 s = 2 669 333.3 → 2 669 334 samples.
  EXPECT_EQ(fs::file_size(dir_ / "carrier.s16"), 2U * 2669334U);
  EXPECT_EQ(fs::file_size(dir_ / "phase.f64"), 8U * 2866500U);
  const std::string resolved = ReadFile(dir_ / "carrier.resolved.yaml");
  EXPECT_NE(resolved.find("# NOT A DISC"), std::string::npos);
  EXPECT_NE(resolved.find("mode: carrier_diagnostic"), std::string::npos);
}

TEST_F(CarrierDiagnosticOutputTest, ResolvedProjectReproducesTheRfByteForByte) {
  ASSERT_EQ(RunExecutable("--project \"" + (dir_ / "carrier.yaml").string() +
                          "\" --threads 1"),
            EXIT_SUCCESS);
  const std::string first = ReadFile(dir_ / "carrier.s16");
  const std::string resolved_text = ReadFile(dir_ / "carrier.resolved.yaml");
  fs::copy_file(dir_ / "carrier.resolved.yaml", dir_ / "rerun.yaml");

  ASSERT_EQ(RunExecutable("--project \"" + (dir_ / "rerun.yaml").string() +
                          "\" --threads 3"),
            EXIT_SUCCESS);
  EXPECT_EQ(ReadFile(dir_ / "carrier.s16"), first);
  EXPECT_EQ(ReadFile(dir_ / "carrier.resolved.yaml"), resolved_text);
}

}  // namespace
}  // namespace ld_creator
