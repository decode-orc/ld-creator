/*
 * File:        test_project_parser.cpp
 * Module:      project
 * Purpose:     Unit tests for project YAML parsing: blocks, defaults, errors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ld_creator/project/project_parser.h"

namespace ld_creator {
namespace {

// The required blocks of a minimal CAV project.
const std::string kRequired =
    "input:\n"
    "  video_path: \"{output}/in.cvbs\"\n"
    "disc_cutter:\n"
    "  format: cav\n"
    "  start_radius_mm: 55.0\n"
    "  track_pitch_um: 1.6667\n"
    "output:\n"
    "  rf_path: \"{output}/out.s16\"\n";

ProjectConfig ParseValid(const std::string& text) {
  const ProjectParseResult result = ParseProjectText(text);
  if (!result.config) {
    std::string errors;
    for (const std::string& error : result.diagnostics.errors) {
      errors += error + "\n";
    }
    ADD_FAILURE() << "unexpected parse failure:\n" << errors;
    return {};
  }
  return *result.config;
}

// True if any error contains `fragment`.
bool HasError(const ProjectParseResult& result, const std::string& fragment) {
  for (const std::string& error : result.diagnostics.errors) {
    if (error.find(fragment) != std::string::npos) {
      return true;
    }
  }
  return false;
}

TEST(ProjectParserTest, AppliesIdealDefaultsWhenOnlyRequiredBlocksAreGiven) {
  const ProjectConfig config = ParseValid(kRequired);

  EXPECT_EQ(config.input.video_path, "{output}/in.cvbs");
  EXPECT_EQ(config.input.first_frame, 0);
  EXPECT_FALSE(config.input.frame_count.has_value());
  EXPECT_TRUE(config.group_delay_predistorter.enabled);
  EXPECT_EQ(config.group_delay_predistorter.delay_table,
            DefaultGroupDelayTable());
  EXPECT_EQ(config.video_pre_emphasis.t1_ns, 320.0);
  EXPECT_EQ(config.video_pre_emphasis.t2_ns, 120.0);
  EXPECT_EQ(config.fm_phase_modulator.blanking_hz, 8.1e6);
  EXPECT_EQ(config.fm_phase_modulator.deviation_hz, 1.7e6);
  EXPECT_EQ(config.fm_phase_modulator.initial_phase_rad, 0.0);
  EXPECT_EQ(config.optical_readout.numerical_aperture, 0.40);
  EXPECT_EQ(config.optical_readout.rim_intensity, 0.23);
  EXPECT_FALSE(config.optical_readout.hfmi.has_value());
  EXPECT_EQ(config.player_front_end.profile, PlayerProfile::kIecReference);
  EXPECT_EQ(config.player_front_end.mtf_equaliser, MtfEqualiser::kNone);
  EXPECT_EQ(config.rf_capture.profile, CaptureProfile::kDomesdayDuplicator);
  EXPECT_EQ(config.rf_capture.level_fraction, 0.75);
  EXPECT_EQ(config.output.mode, OutputMode::kDisc);
  EXPECT_TRUE(config.taps.empty());
  EXPECT_TRUE(config.allow_out_of_tolerance.empty());
  EXPECT_FALSE(config.resolved.has_value());
}

TEST(ProjectParserTest, ReadsEveryKeyOfEveryBlock) {
  const ProjectConfig config = ParseValid(
      "project:\n"
      "  name: Test\n"
      "  version: \"2.1\"\n"
      "  description: Every key\n"
      "input:\n"
      "  video_path: in.cvbs\n"
      "  first_frame: 3\n"
      "  frame_count: 7\n"
      "group_delay_predistorter:\n"
      "  enabled: false\n"
      "  delay_ns: [[0.5, 0], [4.2, -190]]\n"
      "video_pre_emphasis: {t1_ns: 321, t2_ns: 119.5}\n"
      "fm_phase_modulator:\n"
      "  blanking_hz: 8.12e6\n"
      "  deviation_hz: 1700001\n"
      "  initial_phase_rad: -1.5\n"
      "recording_signal_combiner: {}\n"
      "disc_cutter:\n"
      "  format: clv\n"
      "  start_radius_mm: 54\n"
      "  track_pitch_um: 1.6\n"
      "  linear_velocity_m_s: 10.5\n"
      "disc_replicator:\n"
      "playback_mechanics: {}\n"
      "optical_readout: {numerical_aperture: 0.405, rim_intensity: 0.235, "
      "hfmi: 0.25}\n"
      "player_front_end: {profile: iec_reference, mtf_equaliser: inverse}\n"
      "rf_capture: {profile: ideal, level_fraction: 0.5}\n"
      "output: {rf_path: out.s16, mode: carrier_diagnostic}\n"
      "diagnostics:\n"
      "  taps:\n"
      "    - stage: fm_phase_modulator\n"
      "      path: taps/phase\n"
      "      first_frame: 2\n"
      "      frame_count: 3\n"
      "allow_out_of_tolerance: [group_delay_predistorter.enabled]\n");

  EXPECT_EQ(config.project.name, "Test");
  EXPECT_EQ(config.project.version, "2.1");
  EXPECT_EQ(config.project.description, "Every key");
  EXPECT_EQ(config.input.first_frame, 3);
  EXPECT_EQ(config.input.frame_count, 7);
  EXPECT_FALSE(config.group_delay_predistorter.enabled);
  EXPECT_EQ(config.group_delay_predistorter.delay_table,
            (std::vector<GroupDelayPoint>{{0.5, 0.0}, {4.2, -190.0}}));
  EXPECT_EQ(config.video_pre_emphasis.t1_ns, 321.0);
  EXPECT_EQ(config.video_pre_emphasis.t2_ns, 119.5);
  EXPECT_EQ(config.fm_phase_modulator.blanking_hz, 8.12e6);
  EXPECT_EQ(config.fm_phase_modulator.deviation_hz, 1700001.0);
  EXPECT_EQ(config.fm_phase_modulator.initial_phase_rad, -1.5);
  EXPECT_EQ(config.disc_cutter.format, DiscFormat::kClv);
  EXPECT_EQ(config.disc_cutter.start_radius_mm, 54.0);
  EXPECT_EQ(config.disc_cutter.track_pitch_um, 1.6);
  EXPECT_EQ(config.disc_cutter.linear_velocity_m_s, 10.5);
  EXPECT_EQ(config.optical_readout.numerical_aperture, 0.405);
  EXPECT_EQ(config.optical_readout.rim_intensity, 0.235);
  EXPECT_EQ(config.optical_readout.hfmi, 0.25);
  EXPECT_EQ(config.player_front_end.mtf_equaliser, MtfEqualiser::kInverse);
  EXPECT_EQ(config.rf_capture.profile, CaptureProfile::kIdeal);
  EXPECT_EQ(config.rf_capture.level_fraction, 0.5);
  EXPECT_EQ(config.output.rf_path, "out.s16");
  EXPECT_EQ(config.output.mode, OutputMode::kCarrierDiagnostic);
  ASSERT_EQ(config.taps.size(), 1U);
  EXPECT_EQ(config.taps[0].stage, "fm_phase_modulator");
  EXPECT_EQ(config.taps[0].path, "taps/phase");
  EXPECT_EQ(config.taps[0].first_frame, 2);
  EXPECT_EQ(config.taps[0].frame_count, 3);
  EXPECT_EQ(config.allow_out_of_tolerance,
            std::vector<std::string>{"group_delay_predistorter.enabled"});
}

TEST(ProjectParserTest, ReportsEachMissingRequiredBlock) {
  const ProjectParseResult result = ParseProjectText("project:\n  name: x\n");

  EXPECT_FALSE(result.config.has_value());
  EXPECT_TRUE(HasError(result, "input block is required"));
  EXPECT_TRUE(HasError(result, "disc_cutter block is required"));
  EXPECT_TRUE(HasError(result, "output block is required"));
}

TEST(ProjectParserTest, ReportsMissingRequiredKeysByPath) {
  const ProjectParseResult result =
      ParseProjectText("input: {}\ndisc_cutter: {format: cav}\noutput: {}\n");

  EXPECT_TRUE(HasError(result, "input.video_path is required"));
  EXPECT_TRUE(HasError(result, "disc_cutter.start_radius_mm is required"));
  EXPECT_TRUE(HasError(result, "disc_cutter.track_pitch_um is required"));
  EXPECT_TRUE(HasError(result, "output.rf_path is required"));
}

TEST(ProjectParserTest, RejectsUnknownKeysAtEveryLevel) {
  const ProjectParseResult top = ParseProjectText(kRequired + "sections: []\n");
  EXPECT_TRUE(
      HasError(top, "project file contains unsupported key 'sections'"));

  const ProjectParseResult block =
      ParseProjectText(kRequired + "video_pre_emphasis: {tl_ns: 320}\n");
  EXPECT_TRUE(HasError(block,
                       "Project validation error: video_pre_emphasis contains "
                       "unsupported key 'tl_ns'"));

  const ProjectParseResult tap = ParseProjectText(
      kRequired +
      "diagnostics:\n  taps:\n    - {stage: cvbs_input, path: p, size: 2}\n");
  EXPECT_TRUE(
      HasError(tap, "diagnostics.taps[0] contains unsupported key 'size'"));
}

TEST(ProjectParserTest, RejectsSettingsBlocksOfStagesWithNoSettings) {
  EXPECT_TRUE(
      HasError(ParseProjectText(kRequired + "video_rate_converter: {}\n"),
               "unsupported key 'video_rate_converter'"));
  EXPECT_TRUE(HasError(ParseProjectText(kRequired + "recording_limiter: {}\n"),
                       "unsupported key 'recording_limiter'"));
}

TEST(ProjectParserTest, RejectsContentInReservedBlocks) {
  for (const char* block :
       {"recording_signal_combiner", "disc_replicator", "playback_mechanics"}) {
    const ProjectParseResult result =
        ParseProjectText(kRequired + block + ":\n  seed: 1\n");
    EXPECT_TRUE(HasError(result, std::string(block) + " accepts no keys"))
        << block;
  }
}

TEST(ProjectParserTest, RejectsDuplicateKeys) {
  const ProjectParseResult result = ParseProjectText(
      kRequired + "video_pre_emphasis:\n  t1_ns: 320\n  t1_ns: 321\n");

  EXPECT_TRUE(HasError(result, "video_pre_emphasis contains duplicate key"));
}

TEST(ProjectParserTest, RejectsValuesOfTheWrongType) {
  EXPECT_TRUE(HasError(
      ParseProjectText(kRequired + "video_pre_emphasis: {t1_ns: fast}\n"),
      "video_pre_emphasis.t1_ns must be a number"));
  EXPECT_TRUE(HasError(
      ParseProjectText(kRequired + "video_pre_emphasis: {t1_ns: \"320\"}\n"),
      "video_pre_emphasis.t1_ns must be a number"));
  EXPECT_TRUE(HasError(
      ParseProjectText(kRequired + "group_delay_predistorter: {enabled: 1}\n"),
      "group_delay_predistorter.enabled must be true or false"));
  EXPECT_TRUE(HasError(
      ParseProjectText(kRequired +
                       "diagnostics:\n  taps:\n    - {stage: cvbs_input, "
                       "path: p, frame_count: 1.5}\n"),
      "diagnostics.taps[0].frame_count must be an integer"));
  EXPECT_TRUE(HasError(
      ParseProjectText(kRequired +
                       "group_delay_predistorter: {delay_ns: [[0.5, 0, 1]]}\n"),
      "group_delay_predistorter.delay_ns[0] must be a [f_mhz, ns] pair"));
  EXPECT_TRUE(HasError(
      ParseProjectText(kRequired + "fm_phase_modulator: {blanking_hz: .inf}\n"),
      "fm_phase_modulator.blanking_hz must be a number"));
}

TEST(ProjectParserTest, RejectsUnknownEnumeratedValuesListingTheChoices) {
  const ProjectParseResult result =
      ParseProjectText(kRequired + "rf_capture: {profile: cxadc}\n");

  EXPECT_TRUE(HasError(result,
                       "rf_capture.profile = 'cxadc' is not one of: "
                       "domesday_duplicator, ideal"));
}

TEST(ProjectParserTest, ReportsYamlSyntaxErrors) {
  const ProjectParseResult result = ParseProjectText("input: [unclosed\n");

  EXPECT_FALSE(result.config.has_value());
  EXPECT_TRUE(HasError(result, "Project parse error"));
}

TEST(ProjectParserTest, ReadsResolvedBlock) {
  const std::string hash(64, 'a');
  const ProjectConfig config =
      ParseValid(kRequired + "resolved:\n  ld_creator_version: \"abc123\"\n" +
                 "  cvbs_sha256: \"" + hash + "\"\n  meta_sha256: \"" + hash +
                 "\"\n  rf_sha256: \"" + hash + "\"\n");

  if (!config.resolved.has_value()) {
    FAIL();
  }
  EXPECT_EQ(config.resolved.value().ld_creator_version, "abc123");
  EXPECT_EQ(config.resolved.value().cvbs_sha256, hash);
}

}  // namespace
}  // namespace ld_creator
