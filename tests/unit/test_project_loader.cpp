/*
 * File:        test_project_loader.cpp
 * Module:      project
 * Purpose:     Unit tests for path roots and loading project text from a reader
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <map>
#include <string>

#include "ld_creator/project/project_loader.h"

namespace ld_creator {
namespace {

class FakeTextReader final : public ITextFileReader {
 public:
  std::optional<std::string> ReadText(const std::string& path) override {
    const auto it = files.find(path);
    if (it == files.end()) {
      return std::nullopt;
    }
    return it->second;
  }
  std::map<std::string, std::string> files;
};

const PathRoots kProjectOnly{.project_dir = "/work/projects",
                             .output_root = std::nullopt};
const PathRoots kWithOutput{.project_dir = "/work/projects",
                            .output_root = "/scratch/out"};

TEST(ProjectLoaderTest, ResolvesProjectRootToTheProjectDirectory) {
  EXPECT_EQ(ResolveProjectPath("{project}/media/a.cvbs", kWithOutput),
            "/work/projects/media/a.cvbs");
}

TEST(ProjectLoaderTest, ResolvesOutputRootToOutputRootOrProjectDirectory) {
  EXPECT_EQ(ResolveProjectPath("{output}/a.s16", kWithOutput),
            "/scratch/out/a.s16");
  EXPECT_EQ(ResolveProjectPath("{output}/a.s16", kProjectOnly),
            "/work/projects/a.s16");
}

TEST(ProjectLoaderTest, AnchorsRelativeOutputRootAndPathsToProjectDirectory) {
  const PathRoots relative{.project_dir = "/work/projects",
                           .output_root = "../out"};
  EXPECT_EQ(ResolveProjectPath("{output}/a.s16", relative), "/work/out/a.s16");
  EXPECT_EQ(ResolveProjectPath("media/../a.cvbs", kProjectOnly),
            "/work/projects/a.cvbs");
  EXPECT_EQ(ResolveProjectPath("/abs/a.cvbs", kWithOutput), "/abs/a.cvbs");
}

TEST(ProjectLoaderTest, RejectsUnknownPathRoots) {
  EXPECT_FALSE(ResolveProjectPath("{bundled}/a.cvbs", kWithOutput).has_value());
  EXPECT_FALSE(ResolveProjectPath("{output/a.cvbs", kWithOutput).has_value());
}

TEST(ProjectLoaderTest, LoadsResolvesAndValidatesAProject) {
  FakeTextReader reader;
  reader.files["/work/projects/p.yaml"] =
      "input: {video_path: \"{project}/in.cvbs\"}\n"
      "disc_cutter: {format: cav, start_radius_mm: 55, track_pitch_um: "
      "1.6667}\n"
      "output: {rf_path: \"{output}/out.s16\"}\n"
      "diagnostics:\n  taps:\n    - {stage: cvbs_input, path: "
      "\"{output}/t\"}\n";
  ProjectLoader loader(&reader);

  const ProjectLoadResult result =
      loader.Load("/work/projects/p.yaml", kWithOutput);

  if (!result.config.has_value()) {
    FAIL() << result.diagnostics.errors.size();
  }
  const ProjectConfig& config = result.config.value();
  EXPECT_EQ(config.input.video_path, "/work/projects/in.cvbs");
  EXPECT_EQ(config.output.rf_path, "/scratch/out/out.s16");
  EXPECT_EQ(config.taps[0].path, "/scratch/out/t");
  EXPECT_TRUE(config.optical_readout.hfmi.has_value());
}

TEST(ProjectLoaderTest, ReportsUnreadableProjectFile) {
  FakeTextReader reader;
  ProjectLoader loader(&reader);

  const ProjectLoadResult result = loader.Load("/missing.yaml", kProjectOnly);

  EXPECT_FALSE(result.config.has_value());
  ASSERT_EQ(result.diagnostics.errors.size(), 1U);
  EXPECT_EQ(result.diagnostics.errors[0],
            "Cannot read project file: /missing.yaml");
}

TEST(ProjectLoaderTest, ReportsUnknownPathRootWithKeyPath) {
  FakeTextReader reader;
  reader.files["/p.yaml"] =
      "input: {video_path: \"{assets}/in.cvbs\"}\n"
      "disc_cutter: {format: cav, start_radius_mm: 55, track_pitch_um: "
      "1.6667}\n"
      "output: {rf_path: out.s16}\n";
  ProjectLoader loader(&reader);

  const ProjectLoadResult result = loader.Load("/p.yaml", kProjectOnly);

  EXPECT_FALSE(result.config.has_value());
  ASSERT_FALSE(result.diagnostics.errors.empty());
  EXPECT_NE(result.diagnostics.errors[0].find(
                "input.video_path = '{assets}/in.cvbs' uses an unknown path "
                "root"),
            std::string::npos);
}

}  // namespace
}  // namespace ld_creator
