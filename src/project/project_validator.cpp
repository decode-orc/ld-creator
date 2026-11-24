/*
 * File:        project_validator.cpp
 * Module:      project
 * Purpose:     Range, IEC tolerance and cross-key project checks
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/project/project_validator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <set>
#include <string>

#include "ld_creator/optical_readout/readout_transfer.h"
#include "ld_creator/project/number_format.h"
#include "ld_creator/standards/ntsc_timing.h"

namespace ld_creator {

namespace {

constexpr const char* kErrorPrefix = "Project validation error: ";
constexpr const char* kWarningPrefix = "Project validation warning: ";

// A closed interval used for ranges and tolerances.
struct Interval {
  double low;
  double high;

  [[nodiscard]] bool Contains(double value) const {
    return value >= low && value <= high;
  }
  [[nodiscard]] std::string Text() const {
    return FormatDouble(low) + "–" + FormatDouble(high);
  }
};

// Ranges: values the model can compute (docs/reference/project-file.md).
constexpr Interval kPreEmphasisT1Range{50.0, 1000.0};
constexpr Interval kPreEmphasisT2Range{20.0, 500.0};
constexpr Interval kBlankingRange{6.0e6, 10.0e6};
constexpr Interval kDeviationRange{0.5e6, 3.0e6};
constexpr Interval kInitialPhaseRange{-std::numbers::pi, std::numbers::pi};
// IEC 60857-1986 4.5.2 and 4.2: clamping area outer radius to disc radius.
constexpr Interval kStartRadiusRange{41.5, 150.0};
constexpr Interval kTrackPitchRange{0.5, 5.0};
constexpr Interval kLinearVelocityRange{5.0, 20.0};
constexpr Interval kNumericalApertureRange{0.30, 0.60};
constexpr Interval kRimIntensityRange{0.05, 1.0};
constexpr Interval kLevelFractionRange{0.05, 1.0};
constexpr Interval kGroupDelayFrequencyRangeMhz{0.5, 4.2};

// IEC tolerances.
// IEC 60857-1986 9.2.4: t1 = 320 ± 8 ns, t2 = 120 ± 2 ns.
constexpr Interval kPreEmphasisT1Tolerance{312.0, 328.0};
constexpr Interval kPreEmphasisT2Tolerance{118.0, 122.0};
// IEC 60857-1986 9.2.2: 8.1 MHz ± 50 kHz at blanking.
constexpr Interval kBlankingTolerance{8.05e6, 8.15e6};
// IEC 60857-1986 9.2.3: 1.7 MHz ± 35 kHz, sync tip to white.
constexpr Interval kDeviationTolerance{1.665e6, 1.735e6};
// IEC 60857-1986 4.15: track pitch 1.4–2.0 µm.
constexpr Interval kTrackPitchTolerance{1.4, 2.0};
// IEC 60857-1986 4.7.2: CLV linear velocity 10.1–11.4 m/s.
constexpr Interval kLinearVelocityTolerance{10.1, 11.4};
// IEC 60857-1986 11.1.2 (Amendment 2): NA 0.40 ± 0.01.
constexpr Interval kNumericalApertureTolerance{0.39, 0.41};
// IEC 60857-1986 11.1.3: rim intensity 0.23 ± 0.01.
constexpr Interval kRimIntensityTolerance{0.22, 0.24};
// IEC 60857-1986 4.12: the programme area ends at or before 145 mm.
constexpr double kMaximumEndRadiusMm = 145.0;

// IEC 60857-1986 9.1.7, Figure 8 (Amendment 2): tolerance on each table
// delay, in ns (0 at the 0.5 MHz reference).
constexpr double kGroupDelayToleranceNs[] = {0.0, 15.0, 15.0, 15.0, 30.0, 50.0};
// Frequencies are compared to the table's to this precision (MHz).
constexpr double kFrequencyMatchMhz = 1e-9;

// IEC 60857-1986 Amendment 2 Figure 18: HFMI band, read from the figure
// (register S11.hfmi_limits): 0.16–0.40 at 1.4 µm to 0.18–0.42 at 2.0 µm,
// linear in track pitch between them. The figure defines no band outside
// 1.4–2.0 µm.
constexpr double kFigure18PitchLowUm = 1.4;
constexpr double kFigure18PitchHighUm = 2.0;
constexpr Interval kFigure18BandAtLowPitch{0.16, 0.40};
constexpr Interval kFigure18BandAtHighPitch{0.18, 0.42};

// Gathers range and tolerance outcomes for one validation pass.
class Checker {
 public:
  Checker(const std::vector<std::string>& allowed,
          ProjectDiagnostics* diagnostics)
      : allowed_(allowed.begin(), allowed.end()), diagnostics_(diagnostics) {}

  void Error(const std::string& message) {
    diagnostics_->errors.push_back(kErrorPrefix + message);
  }
  void Warning(const std::string& message) {
    diagnostics_->warnings.push_back(kWarningPrefix + message);
  }

  // Returns true when the value is inside its range.
  bool Range(const std::string& path, double value, Interval range) {
    if (range.Contains(value)) {
      return true;
    }
    Error(path + " = " + FormatDouble(value) + " is outside the range " +
          range.Text());
    return false;
  }

  // Records a tolerance outcome for `path`. `description` names the clause
  // and tolerance, for example "IEC 60857 §9.2.4 tolerance 312–328".
  void Tolerance(const std::string& path, bool in_tolerance,
                 const std::string& what, const std::string& description) {
    const bool allowed = allowed_.find(path) != allowed_.end();
    if (in_tolerance) {
      if (allowed) {
        Warning("allow_out_of_tolerance lists " + path +
                ", which is within its IEC tolerance");
      }
      return;
    }
    if (!allowed) {
      Error(what + " is outside the " + description + " (list " + path +
            " in allow_out_of_tolerance to allow it)");
    }
  }

  void NumericTolerance(const std::string& path, double value,
                        Interval tolerance, const std::string& clause) {
    Tolerance(path, tolerance.Contains(value),
              path + " = " + FormatDouble(value),
              clause + " tolerance " + tolerance.Text());
  }

 private:
  std::set<std::string> allowed_;
  ProjectDiagnostics* diagnostics_;
};

bool EndsWith(const std::string& text, const std::string& suffix) {
  return text.size() > suffix.size() &&
         text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool IsSha256Hex(const std::string& text) {
  constexpr std::size_t kSha256HexLength = 64;
  return text.size() == kSha256HexLength &&
         std::all_of(text.begin(), text.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}

void ValidateInput(const InputConfig& input, Checker* checker) {
  if (input.video_path.empty() || !EndsWith(input.video_path, ".cvbs")) {
    checker->Error("input.video_path = '" + input.video_path +
                   "' must name a .cvbs file");
  }
  if (input.first_frame < 0) {
    checker->Error("input.first_frame = " + std::to_string(input.first_frame) +
                   " must be 0 or more");
  }
  if (input.frame_count && *input.frame_count < 1) {
    checker->Error("input.frame_count = " + std::to_string(*input.frame_count) +
                   " must be 1 or more");
  }
}

void ValidateGroupDelay(const GroupDelayPredistorterConfig& config,
                        Checker* checker) {
  const std::string path = "group_delay_predistorter.delay_ns";
  checker->Tolerance("group_delay_predistorter.enabled", config.enabled,
                     "group_delay_predistorter.enabled = false",
                     "IEC 60857 §9.1.7 requirement that pre-distortion "
                     "\"shall\" be applied");

  const std::vector<GroupDelayPoint>& table = config.delay_table;
  bool in_range = table.size() >= 2 && table.front().frequency_mhz == 0.5 &&
                  table.front().delay_ns == 0.0;
  for (std::size_t index = 0; in_range && index < table.size(); ++index) {
    in_range =
        kGroupDelayFrequencyRangeMhz.Contains(table[index].frequency_mhz) &&
        (index == 0 ||
         table[index].frequency_mhz > table[index - 1].frequency_mhz);
  }
  if (!in_range) {
    checker->Error(path +
                   " must start at [0.5, 0] and have at least two points, "
                   "with frequencies ascending within 0.5–4.2 MHz");
    return;
  }

  const std::vector<GroupDelayPoint> reference = DefaultGroupDelayTable();
  bool in_tolerance = table.size() == reference.size();
  for (std::size_t index = 0; in_tolerance && index < table.size(); ++index) {
    in_tolerance =
        std::abs(table[index].frequency_mhz - reference[index].frequency_mhz) <=
            kFrequencyMatchMhz &&
        std::abs(table[index].delay_ns - reference[index].delay_ns) <=
            kGroupDelayToleranceNs[index];
  }
  checker->Tolerance(path, in_tolerance, path,
                     "IEC 60857 §9.1.7 Figure 8 tolerance (the six table "
                     "frequencies, each delay within ±15 ns at 2, 3 and "
                     "3.58 MHz, ±30 ns at 4 MHz and ±50 ns at 4.2 MHz)");
}

void ValidatePreEmphasis(const VideoPreEmphasisConfig& config,
                         Checker* checker) {
  const std::string t1 = "video_pre_emphasis.t1_ns";
  const std::string t2 = "video_pre_emphasis.t2_ns";
  const bool t1_ok = checker->Range(t1, config.t1_ns, kPreEmphasisT1Range);
  const bool t2_ok = checker->Range(t2, config.t2_ns, kPreEmphasisT2Range);
  if (t1_ok && t2_ok && config.t1_ns <= config.t2_ns) {
    checker->Error(t1 + " = " + FormatDouble(config.t1_ns) +
                   " must be greater than " + t2 + " = " +
                   FormatDouble(config.t2_ns));
  }
  if (t1_ok) {
    checker->NumericTolerance(t1, config.t1_ns, kPreEmphasisT1Tolerance,
                              "IEC 60857 §9.2.4");
  }
  if (t2_ok) {
    checker->NumericTolerance(t2, config.t2_ns, kPreEmphasisT2Tolerance,
                              "IEC 60857 §9.2.4");
  }
}

void ValidateFmModulator(const FmPhaseModulatorConfig& config,
                         Checker* checker) {
  const std::string blanking = "fm_phase_modulator.blanking_hz";
  const std::string deviation = "fm_phase_modulator.deviation_hz";
  if (checker->Range(blanking, config.blanking_hz, kBlankingRange)) {
    checker->NumericTolerance(blanking, config.blanking_hz, kBlankingTolerance,
                              "IEC 60857 §9.2.2");
  }
  if (checker->Range(deviation, config.deviation_hz, kDeviationRange)) {
    checker->NumericTolerance(deviation, config.deviation_hz,
                              kDeviationTolerance, "IEC 60857 §9.2.3");
  }
  checker->Range("fm_phase_modulator.initial_phase_rad",
                 config.initial_phase_rad, kInitialPhaseRange);
}

void ValidateDiscCutter(const DiscCutterConfig& config, Checker* checker) {
  checker->Range("disc_cutter.start_radius_mm", config.start_radius_mm,
                 kStartRadiusRange);
  const std::string pitch = "disc_cutter.track_pitch_um";
  if (checker->Range(pitch, config.track_pitch_um, kTrackPitchRange)) {
    checker->NumericTolerance(pitch, config.track_pitch_um,
                              kTrackPitchTolerance, "IEC 60857 §4.15");
  }
  const std::string velocity = "disc_cutter.linear_velocity_m_s";
  if (config.format == DiscFormat::kCav) {
    if (config.linear_velocity_m_s) {
      checker->Error(velocity +
                     " applies to clv only; a CAV disc turns once per frame "
                     "(IEC 60857 §4.7.1)");
    }
    return;
  }
  if (!config.linear_velocity_m_s) {
    checker->Error(velocity + " is required when disc_cutter.format is clv");
    return;
  }
  if (checker->Range(velocity, *config.linear_velocity_m_s,
                     kLinearVelocityRange)) {
    checker->NumericTolerance(velocity, *config.linear_velocity_m_s,
                              kLinearVelocityTolerance, "IEC 60857 §4.7.2");
  }
}

void ValidateOpticalReadout(OpticalReadoutConfig* config, double track_pitch_um,
                            Checker* checker) {
  const std::string na = "optical_readout.numerical_aperture";
  const std::string rim = "optical_readout.rim_intensity";
  const std::string hfmi_path = "optical_readout.hfmi";
  const bool na_ok =
      checker->Range(na, config->numerical_aperture, kNumericalApertureRange);
  const bool rim_ok =
      checker->Range(rim, config->rim_intensity, kRimIntensityRange);
  if (na_ok) {
    checker->NumericTolerance(na, config->numerical_aperture,
                              kNumericalApertureTolerance, "IEC 60857 §11.1.2");
  }
  if (rim_ok) {
    checker->NumericTolerance(rim, config->rim_intensity,
                              kRimIntensityTolerance, "IEC 60857 §11.1.3");
  }
  if (!na_ok || !rim_ok) {
    return;
  }

  // S11 note: the linear readout gives HFMI = m·(4/π)·H(802 pits/mm), and
  // the pit contrast m cannot exceed 1. The default is that limit.
  const double maximum =
      MaximumHfmi(config->numerical_aperture, config->rim_intensity);
  if (!config->hfmi) {
    config->hfmi = maximum;
  }
  const double hfmi = *config->hfmi;
  if (hfmi <= 0.0 || hfmi > maximum) {
    checker->Error(hfmi_path + " = " + FormatDouble(hfmi) +
                   " is outside the range the linear readout can give: above "
                   "0 and at most " +
                   FormatDouble(maximum) +
                   " (pit contrast m ≤ 1 at these optics)");
    return;
  }

  const std::string what = hfmi_path + " = " + FormatDouble(hfmi);
  if (track_pitch_um < kFigure18PitchLowUm ||
      track_pitch_um > kFigure18PitchHighUm) {
    checker->Tolerance(hfmi_path, false, what,
                       "IEC 60857 Figure 18 band, which is defined only for "
                       "track pitches 1.4–2.0 µm (disc_cutter.track_pitch_um "
                       "= " +
                           FormatDouble(track_pitch_um) + ")");
    return;
  }
  const double fraction = (track_pitch_um - kFigure18PitchLowUm) /
                          (kFigure18PitchHighUm - kFigure18PitchLowUm);
  const Interval band{
      kFigure18BandAtLowPitch.low + fraction * (kFigure18BandAtHighPitch.low -
                                                kFigure18BandAtLowPitch.low),
      kFigure18BandAtLowPitch.high + fraction * (kFigure18BandAtHighPitch.high -
                                                 kFigure18BandAtLowPitch.high)};
  checker->Tolerance(hfmi_path, band.Contains(hfmi), what,
                     "IEC 60857 Figure 18 band " + band.Text() +
                         " at track pitch " + FormatDouble(track_pitch_um) +
                         " µm");
}

void ValidateTaps(const std::vector<TapConfig>& taps, Checker* checker) {
  const std::vector<std::string>& names = TapStageNames();
  std::set<std::string> paths;
  for (std::size_t index = 0; index < taps.size(); ++index) {
    const TapConfig& tap = taps[index];
    const std::string path = "diagnostics.taps[" + std::to_string(index) + "]";
    if (std::find(names.begin(), names.end(), tap.stage) == names.end()) {
      std::string accepted;
      for (const std::string& name : names) {
        accepted += (accepted.empty() ? "" : ", ") + name;
      }
      checker->Error(path + ".stage = '" + tap.stage +
                     "' is not a stage with a tap: " + accepted);
    }
    if (tap.path.empty()) {
      checker->Error(path + ".path must not be empty");
    } else if (!paths.insert(tap.path).second) {
      checker->Error(path + ".path = '" + tap.path +
                     "' is used by another tap");
    }
    if (tap.first_frame < 0) {
      checker->Error(path + ".first_frame = " +
                     std::to_string(tap.first_frame) + " must be 0 or more");
    }
    if (tap.frame_count < 1) {
      checker->Error(path + ".frame_count = " +
                     std::to_string(tap.frame_count) + " must be 1 or more");
    }
  }
}

void ValidateAllowList(const std::vector<std::string>& allowed,
                       Checker* checker) {
  const std::vector<std::string>& known = ToleranceKeyPaths();
  std::set<std::string> seen;
  for (const std::string& path : allowed) {
    if (std::find(known.begin(), known.end(), path) == known.end()) {
      checker->Error("allow_out_of_tolerance lists '" + path +
                     "', which is not a key with an IEC tolerance");
    }
    if (!seen.insert(path).second) {
      checker->Error("allow_out_of_tolerance lists '" + path + "' twice");
    }
  }
}

void ValidateResolved(const ResolvedRecord& record, Checker* checker) {
  const std::pair<const char*, const std::string*> hashes[] = {
      {"resolved.cvbs_sha256", &record.cvbs_sha256},
      {"resolved.meta_sha256", &record.meta_sha256},
      {"resolved.rf_sha256", &record.rf_sha256}};
  for (const auto& [path, value] : hashes) {
    if (!IsSha256Hex(*value)) {
      checker->Error(std::string(path) +
                     " must be 64 lower-case hexadecimal digits");
    }
  }
}

}  // namespace

const std::vector<std::string>& ToleranceKeyPaths() {
  static const std::vector<std::string> paths = {
      "group_delay_predistorter.enabled", "group_delay_predistorter.delay_ns",
      "video_pre_emphasis.t1_ns",         "video_pre_emphasis.t2_ns",
      "fm_phase_modulator.blanking_hz",   "fm_phase_modulator.deviation_hz",
      "disc_cutter.start_radius_mm",      "disc_cutter.track_pitch_um",
      "disc_cutter.linear_velocity_m_s",  "optical_readout.numerical_aperture",
      "optical_readout.rim_intensity",    "optical_readout.hfmi"};
  return paths;
}

void ValidateProject(ProjectConfig* config, ProjectDiagnostics* diagnostics) {
  Checker checker(config->allow_out_of_tolerance, diagnostics);
  ValidateAllowList(config->allow_out_of_tolerance, &checker);
  ValidateInput(config->input, &checker);
  ValidateGroupDelay(config->group_delay_predistorter, &checker);
  ValidatePreEmphasis(config->video_pre_emphasis, &checker);
  ValidateFmModulator(config->fm_phase_modulator, &checker);
  ValidateDiscCutter(config->disc_cutter, &checker);
  ValidateOpticalReadout(&config->optical_readout,
                         config->disc_cutter.track_pitch_um, &checker);
  checker.Range("rf_capture.level_fraction", config->rf_capture.level_fraction,
                kLevelFractionRange);
  if (!EndsWith(config->output.rf_path, ".s16")) {
    checker.Error("output.rf_path = '" + config->output.rf_path +
                  "' must name a .s16 file");
  }
  ValidateTaps(config->taps, &checker);
  if (config->resolved) {
    ValidateResolved(*config->resolved, &checker);
  }
}

std::int64_t EffectiveFrameCount(const InputConfig& input,
                                 std::int64_t file_frame_count) {
  if (input.frame_count) {
    return *input.frame_count;
  }
  return std::max<std::int64_t>(file_frame_count - input.first_frame, 0);
}

double EndRadiusMm(const DiscCutterConfig& cutter, std::int64_t frame_count) {
  constexpr double kMetresPerMm = 1e-3;
  constexpr double kMetresPerUm = 1e-6;
  const double r0 = cutter.start_radius_mm * kMetresPerMm;
  const double pitch = cutter.track_pitch_um * kMetresPerUm;
  const auto frames = static_cast<double>(frame_count);
  if (cutter.format == DiscFormat::kCav) {
    // S8 note: r(t) = r0 + p·fr·t, so N revolutions add N pitches.
    return (r0 + pitch * frames) / kMetresPerMm;
  }
  // S8 note: r(t) = √(r0² + p·v·t/π).
  const double velocity = cutter.linear_velocity_m_s.value_or(0.0);
  const double duration_s = frames / ntsc::kFrameRateHz;
  return std::sqrt(r0 * r0 + pitch * velocity * duration_s / std::numbers::pi) /
         kMetresPerMm;
}

void ValidateAgainstInput(const ProjectConfig& config,
                          std::int64_t file_frame_count,
                          ProjectDiagnostics* diagnostics) {
  Checker checker(config.allow_out_of_tolerance, diagnostics);
  const InputConfig& input = config.input;
  if (input.first_frame >= file_frame_count) {
    checker.Error("input.first_frame = " + std::to_string(input.first_frame) +
                  " is beyond the input's " + std::to_string(file_frame_count) +
                  " frames");
    return;
  }
  const std::int64_t frames = EffectiveFrameCount(input, file_frame_count);
  if (input.first_frame + frames > file_frame_count) {
    checker.Error(
        "input.frame_count = " + std::to_string(frames) +
        " from input.first_frame = " + std::to_string(input.first_frame) +
        " runs beyond the input's " + std::to_string(file_frame_count) +
        " frames");
    return;
  }

  for (std::size_t index = 0; index < config.taps.size(); ++index) {
    const TapConfig& tap = config.taps[index];
    if (tap.first_frame + tap.frame_count > frames) {
      checker.Error("diagnostics.taps[" + std::to_string(index) + "] frames " +
                    std::to_string(tap.first_frame) + "–" +
                    std::to_string(tap.first_frame + tap.frame_count - 1) +
                    " are beyond the run's " + std::to_string(frames) +
                    " frames");
    }
  }

  const double end_radius = EndRadiusMm(config.disc_cutter, frames);
  checker.Tolerance(
      "disc_cutter.start_radius_mm", end_radius <= kMaximumEndRadiusMm,
      "disc_cutter.start_radius_mm = " +
          FormatDouble(config.disc_cutter.start_radius_mm) + ": the run of " +
          std::to_string(frames) + " frames ends at radius " +
          FormatDouble(end_radius) + " mm, which",
      "IEC 60857 §4.12 limit of 145 mm");
}

}  // namespace ld_creator
