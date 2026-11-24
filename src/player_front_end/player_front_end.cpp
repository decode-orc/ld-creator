/*
 * File:        player_front_end.cpp
 * Module:      player_front_end
 * Purpose:     S12: the player's photodiode amplifier and RF test point
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/player_front_end/player_front_end.h"

#include <cmath>
#include <numbers>

#include "ld_creator/dsp/kaiser_window.h"
#include "ld_creator/dsp/portable_math.h"
#include "ld_creator/optical_readout/readout_kernel.h"
#include "ld_creator/optical_readout/readout_transfer.h"

namespace ld_creator {

namespace {

// Kaiser (1974): β = 0.1102·(A − 8.7) for A > 50 dB, and the transition
// width × length product D = (A − 7.95)/14.36.
double KaiserBeta() { return 0.1102 * (kEqualiserStopbandDb - 8.7); }
double KaiserTransitionWidthPerM() {
  const double product = (kEqualiserStopbandDb - 7.95) / 14.36;
  return product / (2.0 * kEqualiserHalfWidthM);
}

}  // namespace

double EqualiserBandEdgePerM(double cutoff_per_m) {
  return cutoff_per_m - (0.5 * KaiserTransitionWidthPerM());
}

EdgeResponseTable MakeBandLimitedEdgeResponse(double cutoff_per_m) {
  const double band_edge = EqualiserBandEdgePerM(cutoff_per_m);
  const double beta = KaiserBeta();
  const auto kernel = [band_edge, beta](double u) {
    // 2·fe·sinc(2·fe·u), sinc x = sin(πx)/(πx).
    const double sinc = u == 0.0
                            ? 2.0 * band_edge
                            : SinTurns(band_edge * u) / (std::numbers::pi * u);
    return sinc * KaiserWindow(u / kEqualiserHalfWidthM, beta);
  };
  return {kernel, kEqualiserHalfWidthM, kEdgeTableSpacingM};
}

void ReferenceFrontEnd::Process(const ReadoutBlock& input,
                                PhotocurrentBlock* output) {
  *output = input.photocurrent;
}

void ReferenceFrontEnd::Finish(PhotocurrentBlock* output) {
  *output = PhotocurrentBlock{};
}

InverseMtfEqualiser::InverseMtfEqualiser(double numerical_aperture,
                                         double contrast,
                                         const ISpotTrajectory* trajectory,
                                         ReadoutExtent extent)
    : response_(MakeBandLimitedEdgeResponse(2.0 * numerical_aperture /
                                            kReadoutWavelengthM)),
      renderer_(&response_, trajectory, contrast, extent) {}

void InverseMtfEqualiser::Process(const ReadoutBlock& input,
                                  PhotocurrentBlock* output) {
  renderer_.Process(input.scan, output);
}

void InverseMtfEqualiser::Finish(PhotocurrentBlock* output) {
  renderer_.Finish(output);
}

}  // namespace ld_creator
