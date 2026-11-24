/*
 * File:        disc_replicator.cpp
 * Module:      disc_replicator
 * Purpose:     S9: the pressed disc's pit map from the cut pit map
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/disc_replicator/disc_replicator.h"

namespace ld_creator {

void ExactCopyReplicator::Process(const PitEdgeBlock& input,
                                  PitEdgeBlock* output) {
  *output = input;
  complete_until_m_ = input.complete_until_m;
}

void ExactCopyReplicator::Finish(PitEdgeBlock* output) {
  output->edges.clear();
  output->complete_until_m = complete_until_m_;
}

}  // namespace ld_creator
