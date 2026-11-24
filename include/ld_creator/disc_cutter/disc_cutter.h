/*
 * File:        disc_cutter.h
 * Module:      disc_cutter
 * Purpose:     S8: places recorded edges on the spiral as cut pit edges
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include "ld_creator/disc_cutter/cutting_trajectory.h"
#include "ld_creator/pipeline/stage_interfaces.h"
#include "ld_creator/project/project_config.h"

namespace ld_creator {

// How the cutting-spot exposure moves each edge (S8 note, "Edge profile").
// At the ideal setting the edges are perfect steps at their positions. The
// exposure model that sets these values is later work (plan Task 8.2); it
// has no project key yet.
struct CutEdgeProfile {
  // Register S8.length_bias: every pit is this much longer, in metres, and
  // every land as much shorter. Half is applied at each end of the pit.
  // Its magnitude must stay below the shortest pit or land, so that the
  // edges remain in order.
  double length_bias_m = 0.0;
};

// S8 disc_cutter: D4 → D5. Each recorded edge at time t_k becomes a pit edge
// at arc length x(t_k) and radius r(t_k) on the cutting trajectory, moved
// by the edge profile.
//
// O(1) per edge. Block-size independent. Thread safety: one thread at a
// time.
class DiscCutter final : public IDiscCutter {
 public:
  explicit DiscCutter(const DiscCutterConfig& config,
                      CutEdgeProfile profile = {});

  void Process(const RecordedEdgeBlock& input, PitEdgeBlock* output) override;
  void Finish(PitEdgeBlock* output) override;

  // The cutting trajectory x(t), r(t), which S10 needs (S8 note,
  // "Input/output contract").
  [[nodiscard]] const CuttingTrajectory& Trajectory() const {
    return trajectory_;
  }

 private:
  CuttingTrajectory trajectory_;
  CutEdgeProfile profile_;
  double complete_until_m_ = 0.0;
};

}  // namespace ld_creator
