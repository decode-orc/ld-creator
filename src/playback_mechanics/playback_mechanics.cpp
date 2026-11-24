/*
 * File:        playback_mechanics.cpp
 * Module:      playback_mechanics
 * Purpose:     S10: the spot's path over the pressed disc during playback
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/playback_mechanics/playback_mechanics.h"

#include <algorithm>

namespace ld_creator {

void LockedPlaybackMechanics::Process(const PitEdgeBlock& input,
                                      ScanBlock* output) {
  output->edges = input.edges;
  // Every edge before arc length complete_until_m is in; the spot reaches
  // that arc length at this playback time.
  complete_until_s_ = std::max(
      complete_until_s_,
      std::max(0.0, trajectory_.TimeAtArcLength(input.complete_until_m)));
  output->complete_until_s = complete_until_s_;
}

void LockedPlaybackMechanics::Finish(ScanBlock* output) {
  output->edges.clear();
  output->complete_until_s = complete_until_s_;
}

}  // namespace ld_creator
