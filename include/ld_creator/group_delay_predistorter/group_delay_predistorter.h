/*
 * File:        group_delay_predistorter.h
 * Module:      group_delay_predistorter
 * Purpose:     S3: IEC 60857 9.1.7 group-delay pre-distortion of the video
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>
#include <vector>

#include "ld_creator/pipeline/stage_interfaces.h"
#include "ld_creator/project/project_config.h"

namespace ld_creator {

// Filter length in taps at 4fsc spacing (register S3.filter_taps): 48 taps
// span 3.28 µs. Chosen in docs-tech/user/disc-model/s03-group-delay-
// predistorter.md as the shortest length whose delay error at every §9.1.7
// table frequency is inside the middle third of its tolerance.
inline constexpr int kGroupDelayFilterTaps = 48;

// Least-squares design band (register S3.design_band): the fit runs from 0
// to 6 MHz, the S2 passband. Above the table's last frequency the target
// holds the last delay, with this weight relative to the table range.
inline constexpr double kGroupDelayDesignBandHz = 6.8e6;
inline constexpr double kGroupDelayDesignStepHz = 10.0e3;
inline constexpr double kGroupDelayOutOfTableWeight = 0.1;

// The nominal delay curve relative to 0.5 MHz: a shape-preserving cubic
// through (0, 0) and the table points, held constant beyond them (S3 note).
// Returns ns at `frequency_hz`.
double TargetGroupDelayNs(const std::vector<GroupDelayPoint>& table,
                          double frequency_hz);

// The designed filter. y[m] = Σₖ taps[k]·x[m + advance_samples − 6k] at Fm:
// tap k is k input periods (k / 4fsc) late, and the whole filter is advanced
// by `advance_samples` Fm periods, an exact shift of the time origin.
struct GroupDelayFilter {
  std::vector<double> taps;
  std::int64_t advance_samples = 0;
};

// Designs the filter for `table` (validated: ascending, first point
// [0.5, 0]). Weighted least squares over 0–6 MHz on a 10 kHz grid, solved by
// Cholesky; the taps are then divided by their sum so the DC gain is 1 and
// constant video levels pass unchanged. Deterministic. O(grid · taps²).
GroupDelayFilter DesignGroupDelayFilter(
    const std::vector<GroupDelayPoint>& table);

// The realised group delay of `filter` at `frequency_hz`, in seconds, after
// the advance: what S3 adds to a component at that frequency.
double RealisedGroupDelaySeconds(const GroupDelayFilter& filter,
                                 double frequency_hz);
double RealisedMagnitude(const GroupDelayFilter& filter, double frequency_hz);

// S3 group_delay_predistorter: D2 → D2. With `enabled`, applies the designed
// filter; otherwise passes D2 through unchanged (A/B comparisons only). The
// input is taken as 0 IRE outside the run, and the output has the same
// length as the input.
//
// O(48) multiply-adds per sample. Block-size independent. Thread safety: one
// thread at a time.
class GroupDelayPredistorter final : public IGroupDelayPredistorter {
 public:
  explicit GroupDelayPredistorter(const GroupDelayPredistorterConfig& config);

  void Process(const IreSampleBlock& input, IreSampleBlock* output) override;
  void Finish(IreSampleBlock* output) override;

  [[nodiscard]] const GroupDelayFilter& Filter() const { return filter_; }

 private:
  void Emit(bool at_end, IreSampleBlock* output);

  bool enabled_;
  GroupDelayFilter filter_;
  std::vector<double> history_;
  std::int64_t history_first_ = 0;
  std::int64_t input_count_ = 0;
  std::int64_t next_output_ = 0;
};

}  // namespace ld_creator
