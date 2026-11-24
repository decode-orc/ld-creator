/*
 * File:        test_project_validator.cpp
 * Module:      project
 * Purpose:     Unit tests for ranges, IEC tolerances and the allow list
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <string>

#include "ld_creator/optical_readout/readout_transfer.h"
#include "ld_creator/project/project_validator.h"

namespace ld_creator {
namespace {

// A valid ideal CAV project, as the loader would produce it.
ProjectConfig IdealCav() {
  ProjectConfig config;
  config.input.video_path = "/media/in.cvbs";
  config.disc_cutter.format = DiscFormat::kCav;
  config.disc_cutter.start_radius_mm = 55.0;
  config.disc_cutter.track_pitch_um = 1.6667;
  config.output.rf_path = "/media/out.s16";
  return config;
}

ProjectDiagnostics Validate(ProjectConfig config) {
  ProjectDiagnostics diagnostics;
  ValidateProject(&config, &diagnostics);
  return diagnostics;
}

bool Contains(const std::vector<std::string>& messages,
              const std::string& fragment) {
  for (const std::string& message : messages) {
    if (message.find(fragment) != std::string::npos) {
      return true;
    }
  }
  return false;
}

TEST(ProjectValidatorTest, AcceptsTheIdealDiscWithNoMessages) {
  const ProjectDiagnostics diagnostics = Validate(IdealCav());

  EXPECT_TRUE(diagnostics.errors.empty());
  EXPECT_TRUE(diagnostics.warnings.empty());
}

TEST(ProjectValidatorTest, ComputesDefaultHfmiAsTheLargestWithUnitContrast) {
  ProjectConfig config = IdealCav();
  ProjectDiagnostics diagnostics;
  ValidateProject(&config, &diagnostics);

  if (!config.optical_readout.hfmi.has_value()) {
    FAIL();
  }
  EXPECT_EQ(config.optical_readout.hfmi.value(), MaximumHfmi(0.40, 0.23));
  // S11 note: 0.272 at nominal optics (H = 0.2134 ± 0.002).
  EXPECT_NEAR(config.optical_readout.hfmi.value(),
              4.0 / std::numbers::pi * 0.2134, 4.0 / std::numbers::pi * 0.002);
}

TEST(ProjectValidatorTest, ReportsToleranceErrorWithClauseAndKeyPath) {
  ProjectConfig config = IdealCav();
  config.video_pre_emphasis.t1_ns = 340.0;

  const ProjectDiagnostics diagnostics = Validate(config);

  EXPECT_TRUE(Contains(diagnostics.errors,
                       "Project validation error: video_pre_emphasis.t1_ns = "
                       "340 is outside the IEC 60857 §9.2.4 tolerance "
                       "312–328"));
}

TEST(ProjectValidatorTest, AllowsListedKeyOutsideToleranceButInsideRange) {
  ProjectConfig config = IdealCav();
  config.video_pre_emphasis.t1_ns = 340.0;
  config.allow_out_of_tolerance = {"video_pre_emphasis.t1_ns"};

  const ProjectDiagnostics diagnostics = Validate(config);

  EXPECT_TRUE(diagnostics.errors.empty());
  EXPECT_TRUE(diagnostics.warnings.empty());
}

TEST(ProjectValidatorTest, RangeErrorIsNotExcusedByTheAllowList) {
  ProjectConfig config = IdealCav();
  config.video_pre_emphasis.t1_ns = 2000.0;
  config.allow_out_of_tolerance = {"video_pre_emphasis.t1_ns"};

  const ProjectDiagnostics diagnostics = Validate(config);

  EXPECT_TRUE(Contains(diagnostics.errors,
                       "video_pre_emphasis.t1_ns = 2000 is outside the range "
                       "50–1000"));
}

TEST(ProjectValidatorTest, WarnsWhenAllowListNamesAKeyThatIsInTolerance) {
  ProjectConfig config = IdealCav();
  config.allow_out_of_tolerance = {"fm_phase_modulator.blanking_hz"};

  const ProjectDiagnostics diagnostics = Validate(config);

  EXPECT_TRUE(diagnostics.errors.empty());
  EXPECT_TRUE(Contains(diagnostics.warnings,
                       "allow_out_of_tolerance lists "
                       "fm_phase_modulator.blanking_hz, which is within"));
}

TEST(ProjectValidatorTest, RejectsAllowListPathsWithoutATolerance) {
  ProjectConfig config = IdealCav();
  config.allow_out_of_tolerance = {"rf_capture.level_fraction", "no.such.key"};

  const ProjectDiagnostics diagnostics = Validate(config);

  EXPECT_TRUE(Contains(diagnostics.errors,
                       "'rf_capture.level_fraction', which is not a key with "
                       "an IEC tolerance"));
  EXPECT_TRUE(Contains(diagnostics.errors, "'no.such.key'"));
}

TEST(ProjectValidatorTest, TreatsDisabledPredistortionAsOutOfTolerance) {
  ProjectConfig config = IdealCav();
  config.group_delay_predistorter.enabled = false;

  EXPECT_TRUE(Contains(Validate(config).errors,
                       "group_delay_predistorter.enabled = false is outside"));

  config.allow_out_of_tolerance = {"group_delay_predistorter.enabled"};
  EXPECT_TRUE(Validate(config).errors.empty());
}

TEST(ProjectValidatorTest, ChecksGroupDelayTableAgainstFigureEight) {
  ProjectConfig config = IdealCav();
  // Each delay at the edge of its tolerance is still in tolerance.
  config.group_delay_predistorter.delay_table = {{0.5, 0.0},    {2.0, -30.0},
                                                 {3.0, -30.0},  {3.58, -95.0},
                                                 {4.0, -105.0}, {4.2, -250.0}};
  EXPECT_TRUE(Validate(config).errors.empty());

  config.group_delay_predistorter.delay_table[4].delay_ns = -166.0;
  EXPECT_TRUE(Contains(Validate(config).errors,
                       "group_delay_predistorter.delay_ns is outside the IEC "
                       "60857 §9.1.7"));

  config.group_delay_predistorter.delay_table = {{0.5, 0.0}, {3.58, -80.0}};
  EXPECT_TRUE(Contains(Validate(config).errors,
                       "group_delay_predistorter.delay_ns is outside"));
}

TEST(ProjectValidatorTest, RejectsMalformedGroupDelayTable) {
  ProjectConfig config = IdealCav();
  config.group_delay_predistorter.delay_table = {{0.5, 0.0}, {5.0, -10.0}};
  EXPECT_TRUE(Contains(Validate(config).errors, "must start at [0.5, 0]"));

  config.group_delay_predistorter.delay_table = {{1.0, 0.0}, {2.0, -10.0}};
  EXPECT_TRUE(Contains(Validate(config).errors, "must start at [0.5, 0]"));

  config.group_delay_predistorter.delay_table = {
      {0.5, 0.0}, {3.0, -10.0}, {2.0, -20.0}};
  EXPECT_TRUE(Contains(Validate(config).errors, "ascending"));
}

TEST(ProjectValidatorTest, RequiresT1GreaterThanT2) {
  ProjectConfig config = IdealCav();
  config.video_pre_emphasis.t1_ns = 100.0;
  config.video_pre_emphasis.t2_ns = 120.0;
  config.allow_out_of_tolerance = {"video_pre_emphasis.t1_ns"};

  EXPECT_TRUE(Contains(Validate(config).errors,
                       "video_pre_emphasis.t1_ns = 100 must be greater than"));
}

TEST(ProjectValidatorTest, ChecksFmCarrierTolerances) {
  ProjectConfig config = IdealCav();
  config.fm_phase_modulator.blanking_hz = 8.2e6;
  config.fm_phase_modulator.deviation_hz = 1.6e6;
  const ProjectDiagnostics diagnostics = Validate(config);

  EXPECT_TRUE(Contains(diagnostics.errors,
                       "fm_phase_modulator.blanking_hz = 8200000 is outside "
                       "the IEC 60857 §9.2.2 tolerance 8050000–8150000"));
  EXPECT_TRUE(Contains(diagnostics.errors,
                       "fm_phase_modulator.deviation_hz = 1600000 is outside "
                       "the IEC 60857 §9.2.3 tolerance"));
}

TEST(ProjectValidatorTest, BoundsInitialPhaseToPlusOrMinusPi) {
  ProjectConfig config = IdealCav();
  config.fm_phase_modulator.initial_phase_rad = std::numbers::pi;
  EXPECT_TRUE(Validate(config).errors.empty());

  config.fm_phase_modulator.initial_phase_rad = 3.2;
  EXPECT_TRUE(Contains(Validate(config).errors,
                       "fm_phase_modulator.initial_phase_rad = 3.2 is outside "
                       "the range"));
}

TEST(ProjectValidatorTest, RequiresLinearVelocityForClvOnly) {
  ProjectConfig clv = IdealCav();
  clv.disc_cutter.format = DiscFormat::kClv;
  EXPECT_TRUE(Contains(Validate(clv).errors,
                       "disc_cutter.linear_velocity_m_s is required when "
                       "disc_cutter.format is clv"));

  clv.disc_cutter.linear_velocity_m_s = 10.5;
  EXPECT_TRUE(Validate(clv).errors.empty());

  clv.disc_cutter.linear_velocity_m_s = 12.0;
  EXPECT_TRUE(Contains(Validate(clv).errors,
                       "disc_cutter.linear_velocity_m_s = 12 is outside the "
                       "IEC 60857 §4.7.2 tolerance 10.1–11.4"));

  ProjectConfig cav = IdealCav();
  cav.disc_cutter.linear_velocity_m_s = 10.5;
  EXPECT_TRUE(Contains(Validate(cav).errors, "applies to clv only"));
}

TEST(ProjectValidatorTest, ChecksTrackPitchRangeAndTolerance) {
  ProjectConfig config = IdealCav();
  config.disc_cutter.track_pitch_um = 2.2;
  EXPECT_TRUE(Contains(Validate(config).errors,
                       "disc_cutter.track_pitch_um = 2.2 is outside the IEC "
                       "60857 §4.15 tolerance 1.4–2"));

  config.disc_cutter.track_pitch_um = 6.0;
  EXPECT_TRUE(Contains(Validate(config).errors,
                       "disc_cutter.track_pitch_um = 6 is outside the range"));
}

TEST(ProjectValidatorTest, ChecksOpticsTolerances) {
  ProjectConfig config = IdealCav();
  config.optical_readout.numerical_aperture = 0.45;
  config.optical_readout.rim_intensity = 0.5;
  const ProjectDiagnostics diagnostics = Validate(config);

  EXPECT_TRUE(Contains(diagnostics.errors,
                       "optical_readout.numerical_aperture = 0.45 is outside "
                       "the IEC 60857 §11.1.2 tolerance"));
  EXPECT_TRUE(Contains(diagnostics.errors,
                       "optical_readout.rim_intensity = 0.5 is outside the IEC "
                       "60857 §11.1.3 tolerance"));
}

TEST(ProjectValidatorTest, RejectsHfmiThatNeedsContrastAboveOne) {
  ProjectConfig config = IdealCav();
  config.optical_readout.hfmi = 0.272;

  EXPECT_TRUE(Contains(Validate(config).errors,
                       "optical_readout.hfmi = 0.272 is outside the range the "
                       "linear readout can give"));
}

TEST(ProjectValidatorTest, ChecksHfmiAgainstFigureEighteenBandAtThePitch) {
  ProjectConfig config = IdealCav();
  // At 1.6667 µm the band is 0.1689–0.4089; 0.165 is below it.
  config.optical_readout.hfmi = 0.165;
  EXPECT_TRUE(Contains(Validate(config).errors,
                       "optical_readout.hfmi = 0.165 is outside the IEC 60857 "
                       "Figure 18 band"));

  // At 1.4 µm the band starts at 0.16, so 0.165 is inside it.
  config.disc_cutter.track_pitch_um = 1.4;
  EXPECT_TRUE(Validate(config).errors.empty());
}

TEST(ProjectValidatorTest, ChecksOutputAndInputFileNames) {
  ProjectConfig config = IdealCav();
  config.output.rf_path = "/media/out.wav";
  config.input.video_path = "/media/in.cvbsy";
  const ProjectDiagnostics diagnostics = Validate(config);

  EXPECT_TRUE(Contains(diagnostics.errors, "must name a .s16 file"));
  EXPECT_TRUE(Contains(diagnostics.errors, "must name a .cvbs file"));
}

TEST(ProjectValidatorTest, ChecksTapStagesPathsAndFrames) {
  ProjectConfig config = IdealCav();
  config.taps = {{.stage = "disc_replicator", .path = "/t/a"},
                 {.stage = "cvbs_input", .path = "/t/a"},
                 {.stage = "cvbs_input",
                  .path = "/t/b",
                  .first_frame = -1,
                  .frame_count = 0}};
  const ProjectDiagnostics diagnostics = Validate(config);

  EXPECT_TRUE(Contains(diagnostics.errors,
                       "diagnostics.taps[0].stage = 'disc_replicator' is not a "
                       "stage with a tap"));
  EXPECT_TRUE(Contains(diagnostics.errors,
                       "diagnostics.taps[1].path = '/t/a' is used by another "
                       "tap"));
  EXPECT_TRUE(
      Contains(diagnostics.errors, "diagnostics.taps[2].first_frame = -1"));
  EXPECT_TRUE(
      Contains(diagnostics.errors, "diagnostics.taps[2].frame_count = 0"));
}

TEST(ProjectValidatorTest, RejectsMalformedResolvedHashes) {
  ProjectConfig config = IdealCav();
  config.resolved = ResolvedRecord{.ld_creator_version = "x",
                                   .cvbs_sha256 = std::string(64, 'A'),
                                   .meta_sha256 = std::string(64, '0'),
                                   .rf_sha256 = "abc"};
  const ProjectDiagnostics diagnostics = Validate(config);

  EXPECT_TRUE(Contains(diagnostics.errors, "resolved.cvbs_sha256 must be"));
  EXPECT_FALSE(Contains(diagnostics.errors, "resolved.meta_sha256"));
  EXPECT_TRUE(Contains(diagnostics.errors, "resolved.rf_sha256 must be"));
}

TEST(ProjectValidatorTest, ComputesCavEndRadiusAsOnePitchPerFrame) {
  DiscCutterConfig cutter{.format = DiscFormat::kCav,
                          .start_radius_mm = 55.0,
                          .track_pitch_um = 1.6667};

  EXPECT_DOUBLE_EQ(EndRadiusMm(cutter, 54000), 55.0 + 54000 * 1.6667e-3);
}

TEST(ProjectValidatorTest, ComputesClvEndRadiusFromSweptArea) {
  DiscCutterConfig cutter{.format = DiscFormat::kClv,
                          .start_radius_mm = 54.0,
                          .track_pitch_um = 1.6,
                          .linear_velocity_m_s = 11.0};
  const std::int64_t frames = 30000;
  const double t = frames * 1001.0 / 30000.0;
  const double expected =
      std::sqrt(0.054 * 0.054 + 1.6e-6 * 11.0 * t / std::numbers::pi) * 1e3;

  EXPECT_NEAR(EndRadiusMm(cutter, frames), expected, 1e-9);
}

TEST(ProjectValidatorTest, ChecksRunAgainstInputFrameCount) {
  ProjectConfig config = IdealCav();
  config.input.first_frame = 5;
  config.input.frame_count = 10;
  ProjectDiagnostics diagnostics;
  ValidateAgainstInput(config, 12, &diagnostics);
  EXPECT_TRUE(Contains(diagnostics.errors, "runs beyond the input's 12"));

  config.input.first_frame = 12;
  diagnostics = {};
  ValidateAgainstInput(config, 12, &diagnostics);
  EXPECT_TRUE(Contains(diagnostics.errors,
                       "input.first_frame = 12 is beyond the input's 12"));
}

TEST(ProjectValidatorTest, ChecksTapFramesAgainstTheRun) {
  ProjectConfig config = IdealCav();
  config.taps = {{.stage = "cvbs_input",
                  .path = "/t",
                  .first_frame = 8,
                  .frame_count = 3}};
  ProjectDiagnostics diagnostics;
  ValidateAgainstInput(config, 10, &diagnostics);

  EXPECT_TRUE(Contains(diagnostics.errors,
                       "diagnostics.taps[0] frames 8–10 are beyond the run's "
                       "10 frames"));
}

TEST(ProjectValidatorTest, ChecksEndRadiusAgainstSection412Limit) {
  ProjectConfig config = IdealCav();
  config.disc_cutter.start_radius_mm = 144.0;
  ProjectDiagnostics diagnostics;
  // 1000 frames at 1.6667 µm add 1.67 mm: the run ends at 145.67 mm.
  ValidateAgainstInput(config, 1000, &diagnostics);
  EXPECT_TRUE(Contains(diagnostics.errors, "IEC 60857 §4.12 limit of 145 mm"));

  config.allow_out_of_tolerance = {"disc_cutter.start_radius_mm"};
  diagnostics = {};
  ValidateAgainstInput(config, 1000, &diagnostics);
  EXPECT_TRUE(diagnostics.errors.empty());

  diagnostics = {};
  ValidateAgainstInput(config, 100, &diagnostics);
  EXPECT_TRUE(Contains(diagnostics.warnings,
                       "lists disc_cutter.start_radius_mm, which is within"));
}

}  // namespace
}  // namespace ld_creator
