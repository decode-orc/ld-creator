/*
 * File:        project_parser.cpp
 * Module:      project
 * Purpose:     Parses project YAML text into a typed configuration
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/project/project_parser.h"

#include <yaml-cpp/yaml.h>

#include <charconv>
#include <cstdint>
#include <exception>
#include <set>
#include <string>
#include <system_error>
#include <utility>

#include "ld_creator/project/number_format.h"

namespace ld_creator {

const char* ToString(DiscFormat value) {
  return value == DiscFormat::kCav ? "cav" : "clv";
}

const char* ToString(PlayerProfile /*value*/) { return "iec_reference"; }

const char* ToString(MtfEqualiser value) {
  return value == MtfEqualiser::kNone ? "none" : "inverse";
}

const char* ToString(CaptureProfile value) {
  return value == CaptureProfile::kDomesdayDuplicator ? "domesday_duplicator"
                                                      : "ideal";
}

const char* ToString(OutputMode value) {
  return value == OutputMode::kDisc ? "disc" : "carrier_diagnostic";
}

const std::vector<std::string>& TapStageNames() {
  static const std::vector<std::string> names = {
      "cvbs_input",         "video_rate_converter", "group_delay_predistorter",
      "video_pre_emphasis", "fm_phase_modulator",   "recording_limiter",
      "disc_cutter",        "optical_readout",      "player_front_end",
      "rf_capture"};
  return names;
}

namespace {

constexpr const char* kErrorPrefix = "Project validation error: ";

// Reads typed values from YAML nodes, recording an error naming the key path
// for anything that does not match the schema.
class NodeReader {
 public:
  explicit NodeReader(ProjectDiagnostics* diagnostics)
      : diagnostics_(diagnostics) {}

  void Error(const std::string& message) {
    diagnostics_->errors.push_back(kErrorPrefix + message);
  }

  // A block is a mapping, or null (an empty block). Checks its keys against
  // `allowed` and rejects duplicates. Returns false if it is not a mapping.
  bool CheckMap(const YAML::Node& node, const std::string& path,
                const std::set<std::string>& allowed) {
    if (node.IsNull()) {
      return true;
    }
    if (!node.IsMap()) {
      Error(path + " must be a mapping");
      return false;
    }
    std::set<std::string> seen;
    for (const auto& entry : node) {
      const std::string key = entry.first.Scalar();
      if (!seen.insert(key).second) {
        Error(path + " contains duplicate key '" + key + "'");
      }
      if (allowed.find(key) == allowed.end()) {
        Error(path + " contains unsupported key '" + key + "'");
      }
    }
    return true;
  }

  static std::string Join(const std::string& path, const std::string& key) {
    return path.empty() ? key : path + "." + key;
  }

  // Plain (unquoted) scalars only: a quoted "320" is a string, not a number.
  static bool IsPlainScalar(const YAML::Node& node) {
    return node.IsScalar() && node.Tag() != "!";
  }

  std::optional<double> Double(const YAML::Node& node,
                               const std::string& path) {
    if (IsPlainScalar(node)) {
      if (const std::optional<double> value = ParseDouble(node.Scalar())) {
        return value;
      }
    }
    Error(path + " must be a number");
    return std::nullopt;
  }

  std::optional<std::int64_t> Integer(const YAML::Node& node,
                                      const std::string& path) {
    if (IsPlainScalar(node)) {
      const std::string& text = node.Scalar();
      std::int64_t value = 0;
      const auto result =
          std::from_chars(text.data(), text.data() + text.size(), value);
      if (result.ec == std::errc() && result.ptr == text.data() + text.size()) {
        return value;
      }
    }
    Error(path + " must be an integer");
    return std::nullopt;
  }

  std::optional<bool> Boolean(const YAML::Node& node, const std::string& path) {
    if (IsPlainScalar(node)) {
      if (node.Scalar() == "true") {
        return true;
      }
      if (node.Scalar() == "false") {
        return false;
      }
    }
    Error(path + " must be true or false");
    return std::nullopt;
  }

  std::optional<std::string> String(const YAML::Node& node,
                                    const std::string& path) {
    if (node.IsScalar()) {
      return node.Scalar();
    }
    Error(path + " must be a string");
    return std::nullopt;
  }

  // Reads `key` of `block` into `*target` when present.
  void ReadDouble(const YAML::Node& block, const std::string& path,
                  const char* key, double* target) {
    if (const YAML::Node node = block[key]) {
      if (const auto value = Double(node, Join(path, key))) {
        *target = *value;
      }
    }
  }

  void ReadString(const YAML::Node& block, const std::string& path,
                  const char* key, std::string* target) {
    if (const YAML::Node node = block[key]) {
      if (const auto value = String(node, Join(path, key))) {
        *target = *value;
      }
    }
  }

  void ReadInteger(const YAML::Node& block, const std::string& path,
                   const char* key, std::int64_t* target) {
    if (const YAML::Node node = block[key]) {
      if (const auto value = Integer(node, Join(path, key))) {
        *target = *value;
      }
    }
  }

  // Reads an enumerated string: `choices` maps each accepted text to a value.
  template <typename Enum>
  void ReadChoice(const YAML::Node& block, const std::string& path,
                  const char* key,
                  const std::vector<std::pair<std::string, Enum>>& choices,
                  Enum* target) {
    const YAML::Node node = block[key];
    if (!node) {
      return;
    }
    const std::string key_path = Join(path, key);
    const std::optional<std::string> text = String(node, key_path);
    if (!text) {
      return;
    }
    std::string accepted;
    for (const auto& [name, value] : choices) {
      if (*text == name) {
        *target = value;
        return;
      }
      accepted += (accepted.empty() ? "" : ", ") + name;
    }
    Error(key_path + " = '" + *text + "' is not one of: " + accepted);
  }

  void Require(const YAML::Node& block, const std::string& path,
               const char* key) {
    if (!block[key]) {
      Error(Join(path, key) + " is required");
    }
  }

 private:
  ProjectDiagnostics* diagnostics_;
};

void ParseProjectBlock(const YAML::Node& node, NodeReader* reader,
                       ProjectIdentity* project) {
  if (!reader->CheckMap(node, "project", {"name", "version", "description"}) ||
      node.IsNull()) {
    return;
  }
  reader->ReadString(node, "project", "name", &project->name);
  reader->ReadString(node, "project", "version", &project->version);
  reader->ReadString(node, "project", "description", &project->description);
}

void ParseInput(const YAML::Node& node, NodeReader* reader,
                InputConfig* input) {
  if (!reader->CheckMap(node, "input",
                        {"video_path", "first_frame", "frame_count"})) {
    return;
  }
  reader->Require(node, "input", "video_path");
  if (node.IsNull()) {
    return;
  }
  reader->ReadString(node, "input", "video_path", &input->video_path);
  reader->ReadInteger(node, "input", "first_frame", &input->first_frame);
  if (node["frame_count"]) {
    std::int64_t count = 0;
    reader->ReadInteger(node, "input", "frame_count", &count);
    input->frame_count = count;
  }
}

void ParseGroupDelay(const YAML::Node& node, NodeReader* reader,
                     GroupDelayPredistorterConfig* config) {
  const std::string path = "group_delay_predistorter";
  if (!reader->CheckMap(node, path, {"enabled", "delay_ns"}) || node.IsNull()) {
    return;
  }
  if (const YAML::Node enabled = node["enabled"]) {
    if (const auto value = reader->Boolean(enabled, path + ".enabled")) {
      config->enabled = *value;
    }
  }
  const YAML::Node table = node["delay_ns"];
  if (!table) {
    return;
  }
  const std::string table_path = path + ".delay_ns";
  if (!table.IsSequence()) {
    reader->Error(table_path + " must be a list of [f_mhz, ns] pairs");
    return;
  }
  std::vector<GroupDelayPoint> points;
  for (std::size_t index = 0; index < table.size(); ++index) {
    const std::string point_path =
        table_path + "[" + std::to_string(index) + "]";
    const YAML::Node point = table[index];
    if (!point.IsSequence() || point.size() != 2) {
      reader->Error(point_path + " must be a [f_mhz, ns] pair");
      return;
    }
    const auto frequency = reader->Double(point[0], point_path + "[0]");
    const auto delay = reader->Double(point[1], point_path + "[1]");
    if (!frequency || !delay) {
      return;
    }
    points.push_back({*frequency, *delay});
  }
  config->delay_table = std::move(points);
}

void ParsePreEmphasis(const YAML::Node& node, NodeReader* reader,
                      VideoPreEmphasisConfig* config) {
  const std::string path = "video_pre_emphasis";
  if (!reader->CheckMap(node, path, {"t1_ns", "t2_ns"}) || node.IsNull()) {
    return;
  }
  reader->ReadDouble(node, path, "t1_ns", &config->t1_ns);
  reader->ReadDouble(node, path, "t2_ns", &config->t2_ns);
}

void ParseFmModulator(const YAML::Node& node, NodeReader* reader,
                      FmPhaseModulatorConfig* config) {
  const std::string path = "fm_phase_modulator";
  if (!reader->CheckMap(node, path,
                        {"blanking_hz", "deviation_hz", "initial_phase_rad"}) ||
      node.IsNull()) {
    return;
  }
  reader->ReadDouble(node, path, "blanking_hz", &config->blanking_hz);
  reader->ReadDouble(node, path, "deviation_hz", &config->deviation_hz);
  reader->ReadDouble(node, path, "initial_phase_rad",
                     &config->initial_phase_rad);
}

void ParseDiscCutter(const YAML::Node& node, NodeReader* reader,
                     DiscCutterConfig* config) {
  const std::string path = "disc_cutter";
  if (!reader->CheckMap(node, path,
                        {"format", "start_radius_mm", "track_pitch_um",
                         "linear_velocity_m_s"})) {
    return;
  }
  reader->Require(node, path, "format");
  reader->Require(node, path, "start_radius_mm");
  reader->Require(node, path, "track_pitch_um");
  if (node.IsNull()) {
    return;
  }
  reader->ReadChoice<DiscFormat>(
      node, path, "format",
      {{"cav", DiscFormat::kCav}, {"clv", DiscFormat::kClv}}, &config->format);
  reader->ReadDouble(node, path, "start_radius_mm", &config->start_radius_mm);
  reader->ReadDouble(node, path, "track_pitch_um", &config->track_pitch_um);
  if (node["linear_velocity_m_s"]) {
    double velocity = 0.0;
    reader->ReadDouble(node, path, "linear_velocity_m_s", &velocity);
    config->linear_velocity_m_s = velocity;
  }
}

void ParseOpticalReadout(const YAML::Node& node, NodeReader* reader,
                         OpticalReadoutConfig* config) {
  const std::string path = "optical_readout";
  if (!reader->CheckMap(node, path,
                        {"numerical_aperture", "rim_intensity", "hfmi"}) ||
      node.IsNull()) {
    return;
  }
  reader->ReadDouble(node, path, "numerical_aperture",
                     &config->numerical_aperture);
  reader->ReadDouble(node, path, "rim_intensity", &config->rim_intensity);
  if (node["hfmi"]) {
    double hfmi = 0.0;
    reader->ReadDouble(node, path, "hfmi", &hfmi);
    config->hfmi = hfmi;
  }
}

void ParsePlayerFrontEnd(const YAML::Node& node, NodeReader* reader,
                         PlayerFrontEndConfig* config) {
  const std::string path = "player_front_end";
  if (!reader->CheckMap(node, path, {"profile", "mtf_equaliser"}) ||
      node.IsNull()) {
    return;
  }
  reader->ReadChoice<PlayerProfile>(
      node, path, "profile", {{"iec_reference", PlayerProfile::kIecReference}},
      &config->profile);
  reader->ReadChoice<MtfEqualiser>(
      node, path, "mtf_equaliser",
      {{"none", MtfEqualiser::kNone}, {"inverse", MtfEqualiser::kInverse}},
      &config->mtf_equaliser);
}

void ParseRfCapture(const YAML::Node& node, NodeReader* reader,
                    RfCaptureConfig* config) {
  const std::string path = "rf_capture";
  if (!reader->CheckMap(node, path, {"profile", "level_fraction"}) ||
      node.IsNull()) {
    return;
  }
  reader->ReadChoice<CaptureProfile>(
      node, path, "profile",
      {{"domesday_duplicator", CaptureProfile::kDomesdayDuplicator},
       {"ideal", CaptureProfile::kIdeal}},
      &config->profile);
  reader->ReadDouble(node, path, "level_fraction", &config->level_fraction);
}

void ParseOutput(const YAML::Node& node, NodeReader* reader,
                 OutputConfig* config) {
  const std::string path = "output";
  if (!reader->CheckMap(node, path, {"rf_path", "mode"})) {
    return;
  }
  reader->Require(node, path, "rf_path");
  if (node.IsNull()) {
    return;
  }
  reader->ReadString(node, path, "rf_path", &config->rf_path);
  reader->ReadChoice<OutputMode>(
      node, path, "mode",
      {{"disc", OutputMode::kDisc},
       {"carrier_diagnostic", OutputMode::kCarrierDiagnostic}},
      &config->mode);
}

void ParseDiagnostics(const YAML::Node& node, NodeReader* reader,
                      std::vector<TapConfig>* taps) {
  if (!reader->CheckMap(node, "diagnostics", {"taps"})) {
    return;
  }
  reader->Require(node, "diagnostics", "taps");
  if (node.IsNull()) {
    return;
  }
  const YAML::Node list = node["taps"];
  if (!list) {
    return;
  }
  if (!list.IsSequence()) {
    reader->Error("diagnostics.taps must be a list");
    return;
  }
  for (std::size_t index = 0; index < list.size(); ++index) {
    const std::string path = "diagnostics.taps[" + std::to_string(index) + "]";
    const YAML::Node entry = list[index];
    if (entry.IsNull() ||
        !reader->CheckMap(entry, path,
                          {"stage", "path", "first_frame", "frame_count"})) {
      if (entry.IsNull()) {
        reader->Error(path + " must be a mapping");
      }
      continue;
    }
    reader->Require(entry, path, "stage");
    reader->Require(entry, path, "path");
    TapConfig tap;
    reader->ReadString(entry, path, "stage", &tap.stage);
    reader->ReadString(entry, path, "path", &tap.path);
    reader->ReadInteger(entry, path, "first_frame", &tap.first_frame);
    reader->ReadInteger(entry, path, "frame_count", &tap.frame_count);
    taps->push_back(std::move(tap));
  }
}

void ParseAllowList(const YAML::Node& node, NodeReader* reader,
                    std::vector<std::string>* paths) {
  if (node.IsNull()) {
    return;
  }
  if (!node.IsSequence()) {
    reader->Error("allow_out_of_tolerance must be a list of key paths");
    return;
  }
  for (std::size_t index = 0; index < node.size(); ++index) {
    const std::string path =
        "allow_out_of_tolerance[" + std::to_string(index) + "]";
    if (const auto text = reader->String(node[index], path)) {
      paths->push_back(*text);
    }
  }
}

void ParseResolved(const YAML::Node& node, NodeReader* reader,
                   std::optional<ResolvedRecord>* resolved) {
  const std::string path = "resolved";
  if (!reader->CheckMap(
          node, path,
          {"ld_creator_version", "cvbs_sha256", "meta_sha256", "rf_sha256"})) {
    return;
  }
  for (const char* key :
       {"ld_creator_version", "cvbs_sha256", "meta_sha256", "rf_sha256"}) {
    reader->Require(node, path, key);
  }
  if (node.IsNull()) {
    return;
  }
  ResolvedRecord record;
  reader->ReadString(node, path, "ld_creator_version",
                     &record.ld_creator_version);
  reader->ReadString(node, path, "cvbs_sha256", &record.cvbs_sha256);
  reader->ReadString(node, path, "meta_sha256", &record.meta_sha256);
  reader->ReadString(node, path, "rf_sha256", &record.rf_sha256);
  *resolved = std::move(record);
}

// Blocks reserved for later phases accept no keys yet
// (docs/reference/project-file.md, "Reserved blocks").
void ParseReservedBlock(const YAML::Node& node, const std::string& path,
                        NodeReader* reader) {
  const bool empty = node.IsNull() || (node.IsMap() && node.size() == 0);
  if (!empty) {
    reader->Error(path +
                  " accepts no keys in this version (reserved for a later "
                  "phase)");
  }
}

}  // namespace

ProjectParseResult ParseProjectText(std::string_view text) {
  ProjectParseResult result;
  NodeReader reader(&result.diagnostics);

  YAML::Node root;
  try {
    root = YAML::Load(std::string(text));
  } catch (const YAML::Exception& error) {
    result.diagnostics.errors.push_back(std::string("Project parse error: ") +
                                        error.what());
    return result;
  }

  if (!root.IsMap()) {
    reader.Error("the project file must be a mapping of blocks");
    return result;
  }

  const std::set<std::string> blocks = {"project",
                                        "input",
                                        "group_delay_predistorter",
                                        "video_pre_emphasis",
                                        "fm_phase_modulator",
                                        "recording_signal_combiner",
                                        "disc_cutter",
                                        "disc_replicator",
                                        "playback_mechanics",
                                        "optical_readout",
                                        "player_front_end",
                                        "rf_capture",
                                        "output",
                                        "diagnostics",
                                        "allow_out_of_tolerance",
                                        "resolved"};
  reader.CheckMap(root, "project file", blocks);

  ProjectConfig config;
  for (const char* required : {"input", "disc_cutter", "output"}) {
    if (!root[required]) {
      reader.Error(std::string(required) + " block is required");
    }
  }

  try {
    if (const YAML::Node node = root["project"]) {
      ParseProjectBlock(node, &reader, &config.project);
    }
    if (const YAML::Node node = root["input"]) {
      ParseInput(node, &reader, &config.input);
    }
    if (const YAML::Node node = root["group_delay_predistorter"]) {
      ParseGroupDelay(node, &reader, &config.group_delay_predistorter);
    }
    if (const YAML::Node node = root["video_pre_emphasis"]) {
      ParsePreEmphasis(node, &reader, &config.video_pre_emphasis);
    }
    if (const YAML::Node node = root["fm_phase_modulator"]) {
      ParseFmModulator(node, &reader, &config.fm_phase_modulator);
    }
    for (const char* reserved : {"recording_signal_combiner", "disc_replicator",
                                 "playback_mechanics"}) {
      if (const YAML::Node node = root[reserved]) {
        ParseReservedBlock(node, reserved, &reader);
      }
    }
    if (const YAML::Node node = root["disc_cutter"]) {
      ParseDiscCutter(node, &reader, &config.disc_cutter);
    }
    if (const YAML::Node node = root["optical_readout"]) {
      ParseOpticalReadout(node, &reader, &config.optical_readout);
    }
    if (const YAML::Node node = root["player_front_end"]) {
      ParsePlayerFrontEnd(node, &reader, &config.player_front_end);
    }
    if (const YAML::Node node = root["rf_capture"]) {
      ParseRfCapture(node, &reader, &config.rf_capture);
    }
    if (const YAML::Node node = root["output"]) {
      ParseOutput(node, &reader, &config.output);
    }
    if (const YAML::Node node = root["diagnostics"]) {
      ParseDiagnostics(node, &reader, &config.taps);
    }
    if (const YAML::Node node = root["allow_out_of_tolerance"]) {
      ParseAllowList(node, &reader, &config.allow_out_of_tolerance);
    }
    if (const YAML::Node node = root["resolved"]) {
      ParseResolved(node, &reader, &config.resolved);
    }
  } catch (const YAML::Exception& error) {
    result.diagnostics.errors.push_back(std::string("Project parse error: ") +
                                        error.what());
  }

  if (!result.diagnostics.HasErrors()) {
    result.config = std::move(config);
  }
  return result;
}

}  // namespace ld_creator
