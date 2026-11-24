/*
 * File:        optical_readout.cpp
 * Module:      optical_readout
 * Purpose:     S11: the detector photocurrent as the spot scans the pits
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/optical_readout/optical_readout.h"

#include <cassert>

#include "ld_creator/optical_readout/readout_kernel.h"

namespace ld_creator {

LinearOpticalReadout::LinearOpticalReadout(const OpticalReadoutConfig& config,
                                           const ISpotTrajectory* trajectory,
                                           ReadoutExtent extent)
    : contrast_(PitContrast(config.hfmi.value_or(0.0),
                            config.numerical_aperture, config.rim_intensity)),
      response_(MakeReadoutEdgeResponse(config.numerical_aperture,
                                        config.rim_intensity)),
      renderer_(&response_, trajectory, contrast_, extent) {
  assert(config.hfmi.has_value());
}

void LinearOpticalReadout::Process(const ScanBlock& input,
                                   ReadoutBlock* output) {
  renderer_.Process(input, &output->photocurrent);
  output->scan = input;
}

void LinearOpticalReadout::Finish(ReadoutBlock* output) {
  renderer_.Finish(&output->photocurrent);
  output->scan = ScanBlock{};
}

}  // namespace ld_creator
