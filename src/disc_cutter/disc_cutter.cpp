/*
 * File:        disc_cutter.cpp
 * Module:      disc_cutter
 * Purpose:     S8: places recorded edges on the spiral as cut pit edges
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/disc_cutter/disc_cutter.h"

#include <algorithm>
#include <cmath>

namespace ld_creator {

DiscCutter::DiscCutter(const DiscCutterConfig& config, CutEdgeProfile profile)
    : trajectory_(config), profile_(profile) {}

void DiscCutter::Process(const RecordedEdgeBlock& input, PitEdgeBlock* output) {
  output->edges.clear();
  output->edges.reserve(input.edges.size());
  const double half_bias = 0.5 * profile_.length_bias_m;
  for (const RecordedEdge& edge : input.edges) {
    // A longer pit starts earlier and ends later along the track.
    const double offset =
        edge.polarity == EdgePolarity::kPitStart ? -half_bias : half_bias;
    output->edges.push_back(
        PitEdge{.arc_length_m = trajectory_.ArcLengthAt(edge.time_s) + offset,
                .radius_m = trajectory_.RadiusAt(edge.time_s),
                .polarity = edge.polarity});
  }
  // A later edge can move back by up to half the bias.
  complete_until_m_ = std::max(
      complete_until_m_,
      trajectory_.ArcLengthAt(input.complete_until_s) - std::abs(half_bias));
  output->complete_until_m = complete_until_m_;
}

void DiscCutter::Finish(PitEdgeBlock* output) {
  output->edges.clear();
  output->complete_until_m = complete_until_m_;
}

}  // namespace ld_creator
