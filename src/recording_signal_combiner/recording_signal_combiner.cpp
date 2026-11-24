/*
 * File:        recording_signal_combiner.cpp
 * Module:      recording_signal_combiner
 * Purpose:     S6: slice level for audio and EFM pulse-width modulation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/recording_signal_combiner/recording_signal_combiner.h"

namespace ld_creator {

void PassThroughSignalCombiner::Process(const CarrierPhaseBlock& input,
                                        CarrierPhaseBlock* output) {
  *output = input;
}

void PassThroughSignalCombiner::Finish(CarrierPhaseBlock* output) {
  *output = CarrierPhaseBlock{};
}

}  // namespace ld_creator
