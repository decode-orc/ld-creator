/*
 * File:        resolved_project.cpp
 * Module:      project
 * Purpose:     Writes the resolved project with every effective value
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/project/resolved_project.h"

#include <array>
#include <cstddef>
#include <string>

#include "ld_creator/project/number_format.h"

namespace ld_creator {

namespace {

// A YAML double-quoted scalar (YAML 1.2 §7.3.1 escapes).
std::string Quote(const std::string& text) {
  constexpr const char* kHexDigits = "0123456789ABCDEF";
  constexpr unsigned char kFirstPrintable = 0x20;
  constexpr unsigned char kDelete = 0x7F;
  std::string quoted = "\"";
  for (const char c : text) {
    const auto byte = static_cast<unsigned char>(c);
    if (c == '"' || c == '\\') {
      quoted += '\\';
      quoted += c;
    } else if (c == '\n') {
      quoted += "\\n";
    } else if (c == '\t') {
      quoted += "\\t";
    } else if (byte < kFirstPrintable || byte == kDelete) {
      quoted += "\\x";
      quoted += kHexDigits[byte >> 4];
      quoted += kHexDigits[byte & 0xF];
    } else {
      quoted += c;
    }
  }
  quoted += '"';
  return quoted;
}

class YamlText {
 public:
  void Block(const char* name) {
    text_ += name;
    text_ += ":\n";
  }
  void Key(const char* name, const std::string& value) {
    text_ += "  ";
    text_ += name;
    text_ += ": ";
    text_ += value;
    text_ += '\n';
  }
  void Line(const std::string& line) {
    text_ += line;
    text_ += '\n';
  }
  [[nodiscard]] const std::string& Text() const { return text_; }

 private:
  std::string text_;
};

std::string Bool(bool value) { return value ? "true" : "false"; }

}  // namespace

std::string EmitResolvedProject(const ProjectConfig& config,
                                std::int64_t frame_count,
                                const ResolvedRecord& record) {
  YamlText yaml;
  yaml.Line(
      "# Resolved ld-creator project: every key with its effective value. "
      "Running it");
  yaml.Line("# reproduces the RF byte for byte.");
  yaml.Line("");

  yaml.Block("project");
  yaml.Key("name", Quote(config.project.name));
  yaml.Key("version", Quote(config.project.version));
  yaml.Key("description", Quote(config.project.description));
  yaml.Line("");

  yaml.Block("input");
  yaml.Key("video_path", Quote(config.input.video_path));
  yaml.Key("first_frame", std::to_string(config.input.first_frame));
  yaml.Key("frame_count", std::to_string(frame_count));
  yaml.Line("");

  yaml.Block("group_delay_predistorter");
  yaml.Key("enabled", Bool(config.group_delay_predistorter.enabled));
  std::string table = "[";
  for (const GroupDelayPoint& point :
       config.group_delay_predistorter.delay_table) {
    if (table.size() > 1) {
      table += ", ";
    }
    table += "[" + FormatDouble(point.frequency_mhz) + ", " +
             FormatDouble(point.delay_ns) + "]";
  }
  table += "]";
  yaml.Key("delay_ns", table);
  yaml.Line("");

  yaml.Block("video_pre_emphasis");
  yaml.Key("t1_ns", FormatDouble(config.video_pre_emphasis.t1_ns));
  yaml.Key("t2_ns", FormatDouble(config.video_pre_emphasis.t2_ns));
  yaml.Line("");

  yaml.Block("fm_phase_modulator");
  yaml.Key("blanking_hz", FormatDouble(config.fm_phase_modulator.blanking_hz));
  yaml.Key("deviation_hz",
           FormatDouble(config.fm_phase_modulator.deviation_hz));
  yaml.Key("initial_phase_rad",
           FormatDouble(config.fm_phase_modulator.initial_phase_rad));
  yaml.Line("");

  yaml.Block("disc_cutter");
  yaml.Key("format", ToString(config.disc_cutter.format));
  yaml.Key("start_radius_mm", FormatDouble(config.disc_cutter.start_radius_mm));
  yaml.Key("track_pitch_um", FormatDouble(config.disc_cutter.track_pitch_um));
  if (config.disc_cutter.linear_velocity_m_s) {
    yaml.Key("linear_velocity_m_s",
             FormatDouble(*config.disc_cutter.linear_velocity_m_s));
  }
  yaml.Line("");

  yaml.Block("optical_readout");
  yaml.Key("numerical_aperture",
           FormatDouble(config.optical_readout.numerical_aperture));
  yaml.Key("rim_intensity", FormatDouble(config.optical_readout.rim_intensity));
  yaml.Key("hfmi", FormatDouble(config.optical_readout.hfmi.value_or(0.0)));
  yaml.Line("");

  yaml.Block("player_front_end");
  yaml.Key("profile", ToString(config.player_front_end.profile));
  yaml.Key("mtf_equaliser", ToString(config.player_front_end.mtf_equaliser));
  yaml.Line("");

  yaml.Block("rf_capture");
  yaml.Key("profile", ToString(config.rf_capture.profile));
  yaml.Key("level_fraction", FormatDouble(config.rf_capture.level_fraction));
  yaml.Line("");

  yaml.Block("output");
  yaml.Key("rf_path", Quote(config.output.rf_path));
  yaml.Key("mode", ToString(config.output.mode));
  yaml.Line("");

  yaml.Block("diagnostics");
  if (config.taps.empty()) {
    yaml.Key("taps", "[]");
  } else {
    yaml.Line("  taps:");
    for (const TapConfig& tap : config.taps) {
      yaml.Line("    - stage: " + Quote(tap.stage));
      yaml.Line("      path: " + Quote(tap.path));
      yaml.Line("      first_frame: " + std::to_string(tap.first_frame));
      yaml.Line("      frame_count: " + std::to_string(tap.frame_count));
    }
  }
  yaml.Line("");

  if (config.allow_out_of_tolerance.empty()) {
    yaml.Line("allow_out_of_tolerance: []");
  } else {
    yaml.Block("allow_out_of_tolerance");
    for (const std::string& path : config.allow_out_of_tolerance) {
      yaml.Line("  - " + Quote(path));
    }
  }
  yaml.Line("");

  yaml.Block("resolved");
  yaml.Key("ld_creator_version", Quote(record.ld_creator_version));
  yaml.Key("cvbs_sha256", Quote(record.cvbs_sha256));
  yaml.Key("meta_sha256", Quote(record.meta_sha256));
  yaml.Key("rf_sha256", Quote(record.rf_sha256));
  return yaml.Text();
}

std::string ResolvedProjectPath(const std::string& rf_path) {
  const std::string suffix = ".s16";
  if (rf_path.size() >= suffix.size() &&
      rf_path.compare(rf_path.size() - suffix.size(), suffix.size(), suffix) ==
          0) {
    return rf_path.substr(0, rf_path.size() - suffix.size()) + ".resolved.yaml";
  }
  return rf_path + ".resolved.yaml";
}

}  // namespace ld_creator
