/*
 * File:        player_front_end.h
 * Module:      player_front_end
 * Purpose:     S12: the player's photodiode amplifier and RF test point
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

// The diagnostic equaliser's kernel (S12 note, "MTF equaliser"; register
// S12.equaliser_kernel): a Kaiser-windowed sinc, the line-spread function
// of a low-pass at fe with a smooth transition from fe − Δν/2 to fe + Δν/2.
//   Window: Kaiser, β = 0.1102·(A − 8.7), A = 122 dB, over ±W with
//   W = 20 µm. Transition width Δν = D/(2W), D = (A − 7.95)/14.36 (Kaiser's
//   design formula), so Δν = 199 cycles/mm. (A = 120 dB, the 1e-6 target
//   itself, leaves a stop-band lobe of 1.2e-6; the 2 dB margin covers it.)
//   fe = fc − Δν/2, so the transition ends at the optical cut-off fc and
//   the RF stays inside the band the optics pass.
// With nominal optics the response is 1 ± 1e-6 up to fc − Δν = 0.843·fc,
// which covers every frequency where H > 0.05 (ν < 0.833·fc).
inline constexpr double kEqualiserStopbandDb = 122.0;
inline constexpr double kEqualiserHalfWidthM = 20.0e-6;

// Builds the table at kEdgeTableSpacingM (readout_kernel.h). `cutoff_per_m`
// is fc = 2·NA/λ.
EdgeResponseTable MakeBandLimitedEdgeResponse(double cutoff_per_m);

// The kernel's band edge fe, for `cutoff_per_m` = fc.
double EqualiserBandEdgePerM(double cutoff_per_m);

// S12 player_front_end, profile iec_reference with mtf_equaliser none (the
// ideal setting): a flat, unity-gain, noiseless preamplifier, so D9 = D8,
// the test-point RF. O(1) per sample.
class ReferenceFrontEnd final : public IPlayerFrontEnd {
 public:
  void Process(const ReadoutBlock& input, PhotocurrentBlock* output) override;
  void Finish(PhotocurrentBlock* output) override;
};

// S12 with mtf_equaliser inverse (diagnostic): G = K/H, where K is the
// band-limited kernel's transfer function, so the cascade S11 → S12 is the
// band-limited step instead of the spot's edge response. Computed from the
// D7 edges with that kernel (S12 note, "Implementation"); the D8
// photocurrent is not read.
//
// Ownership: borrows `trajectory`. Thread safety: one thread at a time.
class InverseMtfEqualiser final : public IPlayerFrontEnd {
 public:
  InverseMtfEqualiser(double numerical_aperture, double contrast,
                      const ISpotTrajectory* trajectory, ReadoutExtent extent);

  void Process(const ReadoutBlock& input, PhotocurrentBlock* output) override;
  void Finish(PhotocurrentBlock* output) override;

  [[nodiscard]] const EdgeResponseTable& Response() const { return response_; }

 private:
  EdgeResponseTable response_;
  EdgeSumRenderer renderer_;
};

}  // namespace ld_creator
