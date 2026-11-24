/*
 * File:        pit_train.h
 * Module:      tests
 * Purpose:     Constant-speed trajectories and pit trains for S10–S13 tests
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ld_creator/pipeline/pit_edges.h"
#include "ld_creator/playback_mechanics/spot_trajectory.h"

namespace ld_creator::test {

// x_s(τ) = v·τ.
class ConstantSpeedTrajectory final : public ISpotTrajectory {
 public:
  explicit ConstantSpeedTrajectory(double speed_m_s) : speed_(speed_m_s) {}
  [[nodiscard]] double ArcLengthAt(double tau) const override {
    return speed_ * tau;
  }
  [[nodiscard]] double SpeedAt(double /*tau*/) const override { return speed_; }
  [[nodiscard]] double TimeAtArcLength(double x) const override {
    return x / speed_;
  }

 private:
  double speed_;
};

// A 50 %-duty pit train of spatial period `period_m` over [0, length_m),
// starting on a pit (the first edge ends it at period/4, as S7 gives for
// φ = 0), as consecutive D7 blocks of `edges_per_block` edges. The last
// block's watermark is the end of the record.
inline std::vector<ScanBlock> PitTrain(double period_m, double length_m,
                                       double speed_m_s,
                                       std::size_t edges_per_block) {
  std::vector<ScanBlock> blocks;
  ScanBlock block;
  EdgePolarity polarity = EdgePolarity::kPitEnd;
  for (double x = 0.25 * period_m; x < length_m; x += 0.5 * period_m) {
    block.edges.push_back(
        PitEdge{.arc_length_m = x, .radius_m = 0.0, .polarity = polarity});
    polarity = polarity == EdgePolarity::kPitEnd ? EdgePolarity::kPitStart
                                                 : EdgePolarity::kPitEnd;
    if (block.edges.size() == edges_per_block) {
      block.complete_until_s = x / speed_m_s;
      blocks.push_back(block);
      block = ScanBlock{};
    }
  }
  block.complete_until_s = length_m / speed_m_s;
  blocks.push_back(block);
  return blocks;
}

}  // namespace ld_creator::test
