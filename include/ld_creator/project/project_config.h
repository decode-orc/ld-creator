/*
 * File:        project_config.h
 * Module:      project
 * Purpose:     Typed configuration of a run, one struct per project block
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ld_creator {

// Defaults are the ideal settings from the parameter register
// (docs-tech/user/disc-model/parameter-register.md); the register row is
// named on each.

// `project` block. None of these affects the output.
struct ProjectIdentity {
  std::string name;
  std::string version;
  std::string description;
};

// `input` block (S1).
struct InputConfig {
  // Register S1.video_path. Resolved to an absolute path by the loader.
  std::string video_path;
  // Register S1.first_frame.
  std::int64_t first_frame = 0;
  // Register S1.frame_count: unset means every frame from first_frame.
  std::optional<std::int64_t> frame_count;
};

// One point of the §9.1.7 delay table: frequency in MHz, delay in ns
// relative to 0.5 MHz.
struct GroupDelayPoint {
  double frequency_mhz = 0.0;
  double delay_ns = 0.0;

  bool operator==(const GroupDelayPoint&) const = default;
};

// IEC 60857-1986 9.1.7, Figure 8 (Amendment 2): group-delay pre-distortion
// table, relative to 0.5 MHz (register S3.delay_points).
inline std::vector<GroupDelayPoint> DefaultGroupDelayTable() {
  return {{0.5, 0.0},    {2.0, -15.0},  {3.0, -45.0},
          {3.58, -80.0}, {4.0, -135.0}, {4.2, -200.0}};
}

// `group_delay_predistorter` block (S3).
struct GroupDelayPredistorterConfig {
  // Register S3.enabled: IEC 60857-1986 9.1.7 "shall".
  bool enabled = true;
  // Register S3.delay_points.
  std::vector<GroupDelayPoint> delay_table = DefaultGroupDelayTable();
};

// `video_pre_emphasis` block (S4).
struct VideoPreEmphasisConfig {
  // IEC 60857-1986 9.2.4, Figure 9: zero time constant (register S4.t1).
  double t1_ns = 320.0;
  // IEC 60857-1986 9.2.4, Figure 9: pole time constant (register S4.t2).
  double t2_ns = 120.0;
};

// `fm_phase_modulator` block (S5).
struct FmPhaseModulatorConfig {
  // IEC 60857-1986 9.2.2: carrier at blanking (register
  // S5.blanking_frequency).
  double blanking_hz = 8.1e6;
  // IEC 60857-1986 9.2.3: sync tip to white deviation (register
  // S5.deviation).
  double deviation_hz = 1.7e6;
  // Register S5.initial_phase (declared).
  double initial_phase_rad = 0.0;
};

enum class DiscFormat { kCav, kClv };

// `disc_cutter` block (S8). Required: the standard gives ranges, not
// nominal values (register S8.format, S8.start_radius, S8.track_pitch).
struct DiscCutterConfig {
  DiscFormat format = DiscFormat::kCav;
  double start_radius_mm = 0.0;
  double track_pitch_um = 0.0;
  // Register S8.linear_velocity: set for CLV only.
  std::optional<double> linear_velocity_m_s;
};

// `optical_readout` block (S11).
struct OpticalReadoutConfig {
  // IEC 60857-1986 11.1.2 (Amendment 2): register S11.numerical_aperture.
  double numerical_aperture = 0.40;
  // IEC 60857-1986 11.1.3: register S11.rim_intensity.
  double rim_intensity = 0.23;
  // Register S11.hfmi. Unset in a parsed project means "the largest value
  // with pit contrast m ≤ 1"; validation computes it from the optics keys.
  std::optional<double> hfmi;
};

enum class PlayerProfile { kIecReference };
enum class MtfEqualiser { kNone, kInverse };

// `player_front_end` block (S12).
struct PlayerFrontEndConfig {
  // Register S12.profile.
  PlayerProfile profile = PlayerProfile::kIecReference;
  // Register S12.mtf_equaliser: test-point RF, unequalised.
  MtfEqualiser mtf_equaliser = MtfEqualiser::kNone;
};

enum class CaptureProfile { kDomesdayDuplicator, kIdeal };

// `rf_capture` block (S13).
struct RfCaptureConfig {
  // Register S13.profile.
  CaptureProfile profile = CaptureProfile::kDomesdayDuplicator;
  // Register S13.level_fraction: DdD hardware guide, "Aim for (75%)".
  double level_fraction = 0.75;
};

enum class OutputMode { kDisc, kCarrierDiagnostic };

// `output` block (S14).
struct OutputConfig {
  // Register P.rf_path. Resolved to an absolute path by the loader.
  std::string rf_path;
  // Register P.mode.
  OutputMode mode = OutputMode::kDisc;
};

// One `diagnostics.taps` entry.
struct TapConfig {
  std::string stage;
  // Base path; resolved to an absolute path by the loader.
  std::string path;
  // First frame, relative to input.first_frame.
  std::int64_t first_frame = 0;
  std::int64_t frame_count = 1;
};

// `resolved` block: written by ld-creator into a resolved project, checked
// when one is loaded.
struct ResolvedRecord {
  std::string ld_creator_version;
  std::string cvbs_sha256;
  std::string meta_sha256;
  std::string rf_sha256;
};

// A whole project.
struct ProjectConfig {
  ProjectIdentity project;
  InputConfig input;
  GroupDelayPredistorterConfig group_delay_predistorter;
  VideoPreEmphasisConfig video_pre_emphasis;
  FmPhaseModulatorConfig fm_phase_modulator;
  DiscCutterConfig disc_cutter;
  OpticalReadoutConfig optical_readout;
  PlayerFrontEndConfig player_front_end;
  RfCaptureConfig rf_capture;
  OutputConfig output;
  std::vector<TapConfig> taps;
  // Key paths allowed outside their IEC tolerance.
  std::vector<std::string> allow_out_of_tolerance;
  std::optional<ResolvedRecord> resolved;
};

// Text forms used in the project file.
const char* ToString(DiscFormat value);
const char* ToString(PlayerProfile value);
const char* ToString(MtfEqualiser value);
const char* ToString(CaptureProfile value);
const char* ToString(OutputMode value);

// Stage module names that have a diagnostic tap (each stage note, "Diagnostic
// tap"), in pipeline order.
const std::vector<std::string>& TapStageNames();

// True for the stages a carrier-diagnostic run (output.mode
// carrier_diagnostic) runs, cvbs_input … fm_phase_modulator: the only
// stages whose taps it can write.
bool IsCarrierDiagnosticTapStage(const std::string& stage);

}  // namespace ld_creator
