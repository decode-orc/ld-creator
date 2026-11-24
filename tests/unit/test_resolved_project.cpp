/*
 * File:        test_resolved_project.cpp
 * Module:      project
 * Purpose:     Unit tests: the resolved project is complete and reproducible
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <string>

#include "ld_creator/project/project_parser.h"
#include "ld_creator/project/project_validator.h"
#include "ld_creator/project/resolved_project.h"

namespace ld_creator {
namespace {

ResolvedRecord Record() {
  return {.ld_creator_version = "0123abc",
          .cvbs_sha256 = std::string(64, '1'),
          .meta_sha256 = std::string(64, '2'),
          .rf_sha256 = std::string(64, '3')};
}

// Parses and validates `text`, failing the test on any error.
ProjectConfig Load(const std::string& text) {
  ProjectParseResult parsed = ParseProjectText(text);
  if (!parsed.config) {
    ADD_FAILURE() << "parse failed: "
                  << (parsed.diagnostics.errors.empty()
                          ? ""
                          : parsed.diagnostics.errors[0]);
    return {};
  }
  ProjectDiagnostics diagnostics;
  ValidateProject(&*parsed.config, &diagnostics);
  EXPECT_TRUE(diagnostics.errors.empty())
      << (diagnostics.errors.empty() ? "" : diagnostics.errors[0]);
  return *parsed.config;
}

const std::string kMinimal =
    "input: {video_path: /media/in.cvbs}\n"
    "disc_cutter: {format: cav, start_radius_mm: 55, track_pitch_um: 1.6667}\n"
    "output: {rf_path: /media/out.s16}\n";

TEST(ResolvedProjectTest, ResolvingAResolvedProjectGivesIdenticalBytes) {
  const std::string first = EmitResolvedProject(Load(kMinimal), 100, Record());
  const std::string second = EmitResolvedProject(Load(first), 100, Record());

  EXPECT_EQ(first, second);
}

TEST(ResolvedProjectTest, RoundTripsEveryNonDefaultValueExactly) {
  const std::string text =
      "project: {name: \"Odd \\\"name\\\"\", version: \"1.0\", "
      "description: \"line\\nbreak\"}\n"
      "input: {video_path: /m/in.cvbs, first_frame: 4, frame_count: 9}\n"
      "group_delay_predistorter: {delay_ns: [[0.5, 0], [2.0, -15.1], [3.0, "
      "-45], [3.58, -80], [4.0, -135], [4.2, -200.000000001]]}\n"
      "video_pre_emphasis: {t1_ns: 320.1, t2_ns: 119.99999999999999}\n"
      "fm_phase_modulator: {blanking_hz: 8100000.3, initial_phase_rad: "
      "0.1}\n"
      "disc_cutter: {format: clv, start_radius_mm: 54.1, track_pitch_um: "
      "1.6, linear_velocity_m_s: 10.75}\n"
      "optical_readout: {hfmi: 0.25}\n"
      "player_front_end: {mtf_equaliser: inverse}\n"
      "rf_capture: {profile: ideal, level_fraction: 0.3}\n"
      "output: {rf_path: /m/out.s16, mode: carrier_diagnostic}\n"
      "diagnostics:\n  taps:\n"
      "    - {stage: cvbs_input, path: /m/t1, first_frame: 1, frame_count: "
      "2}\n"
      "    - {stage: fm_phase_modulator, path: /m/t2}\n";
  const ProjectConfig original = Load(text);
  const std::string emitted = EmitResolvedProject(original, 9, Record());
  const ProjectConfig reloaded = Load(emitted);

  EXPECT_EQ(reloaded.project.name, "Odd \"name\"");
  EXPECT_EQ(reloaded.project.description, "line\nbreak");
  EXPECT_EQ(reloaded.input.frame_count, 9);
  EXPECT_EQ(reloaded.group_delay_predistorter.delay_table,
            original.group_delay_predistorter.delay_table);
  EXPECT_EQ(reloaded.video_pre_emphasis.t2_ns,
            original.video_pre_emphasis.t2_ns);
  EXPECT_EQ(reloaded.fm_phase_modulator.blanking_hz, 8100000.3);
  EXPECT_EQ(reloaded.disc_cutter.linear_velocity_m_s, 10.75);
  EXPECT_EQ(reloaded.optical_readout.hfmi, 0.25);
  EXPECT_EQ(reloaded.player_front_end.mtf_equaliser, MtfEqualiser::kInverse);
  EXPECT_EQ(reloaded.output.mode, OutputMode::kCarrierDiagnostic);
  ASSERT_EQ(reloaded.taps.size(), 2U);
  EXPECT_EQ(reloaded.taps[1].frame_count, 1);
  if (!reloaded.resolved.has_value()) {
    FAIL();
  }
  EXPECT_EQ(reloaded.resolved.value().rf_sha256, std::string(64, '3'));
  EXPECT_EQ(EmitResolvedProject(reloaded, 9, Record()), emitted);
}

TEST(ResolvedProjectTest, WritesComputedHfmiAndEffectiveFrameCount) {
  const std::string emitted = EmitResolvedProject(Load(kMinimal), 42, Record());

  EXPECT_NE(emitted.find("  frame_count: 42\n"), std::string::npos);
  EXPECT_NE(emitted.find("  hfmi: 0.27"), std::string::npos);
  EXPECT_NE(emitted.find("  mode: disc\n"), std::string::npos);
  EXPECT_NE(emitted.find("taps: []"), std::string::npos);
  EXPECT_NE(emitted.find("allow_out_of_tolerance: []"), std::string::npos);
  EXPECT_EQ(emitted.find("linear_velocity_m_s"), std::string::npos);
}

TEST(ResolvedProjectTest, WritesBlocksInReferenceOrder) {
  const std::string emitted = EmitResolvedProject(Load(kMinimal), 1, Record());
  std::size_t previous = 0;
  for (const char* block :
       {"\nproject:", "\ninput:", "\ngroup_delay_predistorter:",
        "\nvideo_pre_emphasis:", "\nfm_phase_modulator:", "\ndisc_cutter:",
        "\noptical_readout:", "\nplayer_front_end:", "\nrf_capture:",
        "\noutput:", "\ndiagnostics:", "\nallow_out_of_tolerance:",
        "\nresolved:"}) {
    const std::size_t position = emitted.find(block);
    ASSERT_NE(position, std::string::npos) << block;
    EXPECT_GT(position, previous) << block;
    previous = position;
  }
}

TEST(ResolvedProjectTest, NamesResolvedProjectBesideTheRf) {
  EXPECT_EQ(ResolvedProjectPath("/out/disc.s16"), "/out/disc.resolved.yaml");
}

}  // namespace
}  // namespace ld_creator
