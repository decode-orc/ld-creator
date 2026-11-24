/*
 * File:        fir_response.cpp
 * Module:      dsp
 * Purpose:     Frequency response and group delay of a tapped delay line
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/dsp/fir_response.h"

#include <cmath>
#include <cstddef>

#include "ld_creator/dsp/portable_math.h"

namespace ld_creator {

FirResponsePoint FirResponseAt(std::span<const double> taps,
                               double tap_interval_s, double frequency_hz) {
  // H = A − jB, and Σ cₖ kT e^{−jωkT} = C − jE.
  double a = 0.0;
  double b = 0.0;
  double c = 0.0;
  double e = 0.0;
  for (std::size_t k = 0; k < taps.size(); ++k) {
    const double delay = static_cast<double>(k) * tap_interval_s;
    const SinCosValue rotation = SinCosTurns(frequency_hz * delay);
    a += taps[k] * rotation.cos;
    b += taps[k] * rotation.sin;
    c += taps[k] * delay * rotation.cos;
    e += taps[k] * delay * rotation.sin;
  }
  const double power = (a * a) + (b * b);
  return {.magnitude = std::sqrt(power),
          .group_delay_s = ((c * a) + (e * b)) / power};
}

}  // namespace ld_creator
