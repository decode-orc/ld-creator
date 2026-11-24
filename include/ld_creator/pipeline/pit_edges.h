/*
 * File:        pit_edges.h
 * Module:      pipeline
 * Purpose:     Pit-edge event-list representations (D4, D5, D6, D7)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>
#include <vector>

namespace ld_creator {

// Which side of a pit an edge is. Edges of a stream alternate, starting with
// whichever the signal begins with (S7 note).
enum class EdgePolarity : std::int8_t {
  kPitEnd = -1,
  kPitStart = 1,
};

// One recorded transition: recording time in seconds from the D1 origin.
struct RecordedEdge {
  double time_s = 0.0;
  EdgePolarity polarity = EdgePolarity::kPitStart;
};

// D4 (S7 output): recorded transitions in time order. Every edge with time
// before `complete_until_s` has been delivered in this block or an earlier
// one, so a consumer may finalise its output up to that time.
struct RecordedEdgeBlock {
  std::vector<RecordedEdge> edges;
  double complete_until_s = 0.0;
};

// One pit edge on the disc: arc length along the spiral and radius, both in
// metres, with x = 0 at the cutting position of t = 0 (S8 note).
struct PitEdge {
  double arc_length_m = 0.0;
  double radius_m = 0.0;
  EdgePolarity polarity = EdgePolarity::kPitStart;
};

// D5 (S8 output, cut pit map) and D6 (S9 output, pressed pit map): pit edges
// in arc-length order. Every edge with arc length before
// `complete_until_m` has been delivered. The cutting trajectory x(t), r(t)
// is a property of the cutter, not of a block, and is passed to S10 when the
// pipeline is built (S8 note). Edge geometry attributes are added to D6 in
// the deterministic-impairment phase.
struct PitEdgeBlock {
  std::vector<PitEdge> edges;
  double complete_until_m = 0.0;
};

// D7 (S10 output): the pit edges the readout spot passes, with the playback
// interval the block covers. The spot trajectory x_s(τ), ρ(τ) is a property
// of the playback mechanics and is passed to S11 when the pipeline is built
// (S10 note). Every edge the spot can reach before `complete_until_s` (in
// playback time τ, seconds) has been delivered.
struct ScanBlock {
  std::vector<PitEdge> edges;
  double complete_until_s = 0.0;
};

}  // namespace ld_creator
