/*
 * File:        disc_replicator.h
 * Module:      disc_replicator
 * Purpose:     S9: the pressed disc's pit map from the cut pit map
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include "ld_creator/pipeline/stage_interfaces.h"

namespace ld_creator {

// S9 disc_replicator at its ideal setting: the pressed disc is an exact copy
// of the cut pit map, so D6 = D5 (S9 note; register S9.copy). Replication
// impairments implement the same interface (plan Task 8.3).
//
// O(1) per edge. Block-size independent. Thread safety: one thread at a
// time.
class ExactCopyReplicator final : public IDiscReplicator {
 public:
  void Process(const PitEdgeBlock& input, PitEdgeBlock* output) override;
  void Finish(PitEdgeBlock* output) override;

 private:
  double complete_until_m_ = 0.0;
};

}  // namespace ld_creator
