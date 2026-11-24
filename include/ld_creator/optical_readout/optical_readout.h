/*
 * File:        optical_readout.h
 * Module:      optical_readout
 * Purpose:     S11: the detector photocurrent as the spot scans the pits
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include "ld_creator/dsp/edge_response_table.h"
#include "ld_creator/optical_readout/edge_sum_renderer.h"
#include "ld_creator/pipeline/stage_interfaces.h"
#include "ld_creator/playback_mechanics/spot_trajectory.h"
#include "ld_creator/project/project_config.h"

namespace ld_creator {

// S11 optical_readout, linear model: D7 → D8 at Fr = 80 MHz,
//   i(τ) = 1 − m·Σ σ_k·ESF(x_s(τ) − x_k)
// rendered as in EdgeSumRenderer with the readout kernel
// (MakeReadoutEdgeResponse) and m from the configured HFMI (S11 note). The
// D7 block is passed on with D8 for S12.
//
// Construction builds the edge-response table (about 0.1 s). Block-size
// independent. Ownership: borrows `trajectory`. Thread safety: one thread
// at a time.
class LinearOpticalReadout final : public IOpticalReadout {
 public:
  // `config.hfmi` must be set (the validator fills it).
  LinearOpticalReadout(const OpticalReadoutConfig& config,
                       const ISpotTrajectory* trajectory, ReadoutExtent extent);

  void Process(const ScanBlock& input, ReadoutBlock* output) override;
  void Finish(ReadoutBlock* output) override;

  [[nodiscard]] double Contrast() const { return contrast_; }
  [[nodiscard]] const EdgeResponseTable& Response() const { return response_; }

 private:
  double contrast_;
  EdgeResponseTable response_;
  EdgeSumRenderer renderer_;
};

}  // namespace ld_creator
