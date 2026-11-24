/*
 * File:        test_run_setup.cpp
 * Module:      pipeline
 * Purpose:     Unit tests for loading a project against its input media
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "ld_creator/pipeline/run_setup.h"
#include "ld_creator/standards/ntsc_timing.h"
#include "memory_io.h"

namespace ld_creator {
namespace {

class FakeTextReader final : public ITextFileReader {
 public:
  std::optional<std::string> ReadText(const std::string& path) override {
    const auto it = files.find(path);
    return it == files.end() ? std::nullopt
                             : std::optional<std::string>(it->second);
  }
  std::map<std::string, std::string> files;
};

class FakeMetadataReader final : public ICvbsMetadataReader {
 public:
  CvbsMetadataResult Read(const std::string& meta_path) override {
    if (meta_path != "/m/in.meta") {
      return {.metadata = std::nullopt, .error = "no metadata " + meta_path};
    }
    return {.metadata = metadata, .error = {}};
  }
  CvbsMetadata metadata{.preset = "NTSC",
                        .sample_encoding_preset = "CVBS_S16_4FSC",
                        .signal_state_preset = "STANDARD_STABLE_LOCKED",
                        .signal_type = "composite",
                        .number_of_sequential_frames = 3};
};

class FakeByteSources final : public IByteSourceFactory {
 public:
  std::unique_ptr<IByteSource> Open(const std::string& path) override {
    const auto it = files.find(path);
    if (it == files.end()) {
      throw std::runtime_error("cannot open input video " + path);
    }
    return std::make_unique<test::MemoryByteSource>(it->second);
  }
  std::map<std::string, std::vector<std::byte>> files;
};

constexpr std::size_t kFrameBytes =
    static_cast<std::size_t>(ntsc::kSamplesPerFrame) * 2;

// SHA-256 of `count` zero bytes, for the resolved-hash tests.
std::string ZeroHash(std::size_t count) {
  test::MemoryByteSource source{std::vector<std::byte>(count)};
  return HashByteSource(&source);
}

class RunSetupTest : public ::testing::Test {
 protected:
  RunSetupTest() {
    bytes_.files["/m/in.cvbs"] = std::vector<std::byte>(3 * kFrameBytes);
    bytes_.files["/m/in.meta"] = std::vector<std::byte>(16);
  }

  RunSetupResult Prepare(const std::string& extra = "") {
    reader_.files["/p/project.yaml"] =
        "input: {video_path: /m/in.cvbs}\n"
        "disc_cutter: {format: cav, start_radius_mm: 55, track_pitch_um: "
        "1.6667}\n"
        "output: {rf_path: \"{output}/out.s16\"}\n" +
        extra;
    return PrepareRun("/p/project.yaml",
                      {.project_dir = "/p", .output_root = "/o"}, "build1",
                      {.project_reader = &reader_,
                       .metadata_reader = &metadata_,
                       .byte_sources = &bytes_});
  }

  static bool Contains(const std::vector<std::string>& messages,
                       const std::string& fragment) {
    for (const std::string& message : messages) {
      if (message.find(fragment) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  FakeTextReader reader_;
  FakeMetadataReader metadata_;
  FakeByteSources bytes_;
};

TEST_F(RunSetupTest, PreparesAValidRunWithEncodingAndFrameCounts) {
  const RunSetupResult result = Prepare();

  if (!result.run.has_value()) {
    FAIL() << (result.diagnostics.errors.empty()
                   ? ""
                   : result.diagnostics.errors[0]);
  }
  EXPECT_EQ(result.run.value().encoding, SampleEncoding::kS16);
  EXPECT_EQ(result.run.value().file_frame_count, 3);
  EXPECT_EQ(result.run.value().frame_count, 3);
  EXPECT_EQ(result.run.value().meta_path, "/m/in.meta");
  EXPECT_EQ(result.run.value().config.output.rf_path, "/o/out.s16");
}

TEST_F(RunSetupTest, ReportsInputThatIsNotNtsc) {
  metadata_.metadata.preset = "PAL";

  const RunSetupResult result = Prepare();

  EXPECT_FALSE(result.run.has_value());
  EXPECT_TRUE(Contains(result.diagnostics.errors,
                       "Input error: input video standard is 'PAL'"));
}

TEST_F(RunSetupTest, ReportsMissingVideoFile) {
  bytes_.files.erase("/m/in.cvbs");

  EXPECT_TRUE(Contains(Prepare().diagnostics.errors,
                       "cannot open input video /m/in.cvbs"));
}

TEST_F(RunSetupTest, AppliesInputDependentChecks) {
  const RunSetupResult beyond = Prepare(
      "diagnostics:\n  taps:\n    - {stage: cvbs_input, path: t, "
      "first_frame: 3}\n");
  EXPECT_FALSE(beyond.run.has_value());
  EXPECT_TRUE(Contains(beyond.diagnostics.errors, "beyond the run's 3 frames"));
}

TEST_F(RunSetupTest, AcceptsMatchingResolvedHashesAndWarnsOnVersion) {
  const std::string resolved =
      "resolved:\n  ld_creator_version: \"older\"\n  cvbs_sha256: \"" +
      ZeroHash(3 * kFrameBytes) + "\"\n  meta_sha256: \"" + ZeroHash(16) +
      "\"\n  rf_sha256: \"" + std::string(64, '0') + "\"\n";

  const RunSetupResult result = Prepare(resolved);

  if (!result.run.has_value()) {
    FAIL();
  }
  EXPECT_TRUE(Contains(result.diagnostics.warnings,
                       "resolved.ld_creator_version = 'older' differs from "
                       "this build ('build1')"));
}

TEST_F(RunSetupTest, RejectsResolvedProjectWhoseInputChanged) {
  const std::string resolved =
      "resolved:\n  ld_creator_version: \"build1\"\n  cvbs_sha256: \"" +
      ZeroHash(3 * kFrameBytes) + "\"\n  meta_sha256: \"" +
      std::string(64, 'f') + "\"\n  rf_sha256: \"" + std::string(64, '0') +
      "\"\n";

  const RunSetupResult result = Prepare(resolved);

  EXPECT_FALSE(result.run.has_value());
  EXPECT_TRUE(Contains(result.diagnostics.errors,
                       "/m/in.meta does not match resolved.meta_sha256"));
  EXPECT_TRUE(result.diagnostics.warnings.empty());
}

}  // namespace
}  // namespace ld_creator
