/*
 * File:        recording_signal_combiner.h
 * Module:      recording_signal_combiner
 * Purpose:     S6: slice level for audio and EFM pulse-width modulation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include "ld_creator/pipeline/stage_interfaces.h"

namespace ld_creator {

// S6 recording_signal_combiner, video only: D3 → D3 unchanged. The slice
// level s is 0 (register S6.slice_level), so S7 cuts at φ = (k + ½)π. Audio
// and EFM sources will implement IRecordingSignalCombiner by setting s
// (IEC 60857-1986 7.2.1; Amendment 2 13.1.5).
//
// O(1) per sample. Block-size independent. Thread safety: one thread at a
// time.
class PassThroughSignalCombiner final : public IRecordingSignalCombiner {
 public:
  void Process(const CarrierPhaseBlock& input,
               CarrierPhaseBlock* output) override;
  void Finish(CarrierPhaseBlock* output) override;
};

}  // namespace ld_creator
