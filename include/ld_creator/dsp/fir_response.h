/*
 * File:        fir_response.h
 * Module:      dsp
 * Purpose:     Frequency response and group delay of a tapped delay line
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <span>

namespace ld_creator {

struct FirResponsePoint {
  double magnitude = 0.0;
  // −dθ/dω, in seconds, of the filter as written (no advance removed).
  double group_delay_s = 0.0;
};

// Response at `frequency_hz` of y(t) = Σₖ taps[k]·x(t − k·tap_interval_s).
// With H = Σ cₖ e^{−jωkT}, the group delay is Re[(Σ cₖ kT e^{−jωkT}) / H].
// Pure function, O(taps).
FirResponsePoint FirResponseAt(std::span<const double> taps,
                               double tap_interval_s, double frequency_hz);

}  // namespace ld_creator
