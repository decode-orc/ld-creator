/*
 * File:        readout_transfer.cpp
 * Module:      optical_readout
 * Purpose:     Linear readout transfer function of the reference optics
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/optical_readout/readout_transfer.h"

#include <cmath>
#include <cstddef>
#include <numbers>

#include "ld_creator/dsp/gauss_legendre.h"
#include "ld_creator/dsp/portable_math.h"

namespace ld_creator {

namespace {

// Quadrature order. 64 points integrate the smooth integrand far below the
// ±0.002 numerical tolerance of register S11.mtf_at_reference.
constexpr std::size_t kQuadratureOrder = 64;

const GaussLegendreRule& Rule() {
  static const GaussLegendreRule rule = MakeGaussLegendreRule(kQuadratureOrder);
  return rule;
}

// ∫_{−y}^{y} exp(−β·u²) du.
double GaussianChordIntegral(double beta, double half_chord) {
  if (beta == 0.0) {
    return 2.0 * half_chord;
  }
  const double root = std::sqrt(beta);
  return std::sqrt(std::numbers::pi / beta) * PortableErf(root * half_chord);
}

// ∫ over the unit disc of exp(−β·ρ²).
double PupilPower(double beta) {
  if (beta == 0.0) {
    return std::numbers::pi;
  }
  return std::numbers::pi * (-PortableExpm1(-beta)) / beta;
}

}  // namespace

double ReadoutTransfer(double frequency_per_m, double numerical_aperture,
                       double rim_intensity, double wavelength_m) {
  const double nu =
      std::abs(frequency_per_m) * wavelength_m / numerical_aperture;
  if (nu >= 2.0) {
    return 0.0;
  }
  // S11 note: β = −ln(rim-to-centre intensity ratio) (register S11.beta).
  const double beta = -PortableLog(rim_intensity);
  const double theta_max = PortableAcos(nu / 2.0);
  const GaussLegendreRule& rule = Rule();
  double sum = 0.0;
  for (std::size_t i = 0; i < kQuadratureOrder; ++i) {
    const double theta = (rule.nodes[i] + 1.0) * 0.5 * theta_max;
    const SinCosValue angle = SinCosRadians(theta);
    const double x = angle.cos;
    const double half_chord = angle.sin;
    const double along =
        PortableExp(-0.5 * beta * (x * x + (x - nu) * (x - nu)));
    sum += rule.weights[i] * along * GaussianChordIntegral(beta, half_chord) *
           angle.sin;
  }
  // Both halves of the lens, the 0.5·θmax Jacobian of the node mapping, and
  // normalisation by the pupil power so that H(0) = 1.
  return 2.0 * sum * 0.5 * theta_max / PupilPower(beta);
}

double MaximumHfmi(double numerical_aperture, double rim_intensity) {
  // S11 note: HFMI = m·(4/π)·H(802 pits/mm), with m ≤ 1.
  return 4.0 / std::numbers::pi *
         ReadoutTransfer(kHfmiReferenceFrequencyPerM, numerical_aperture,
                         rim_intensity, kReadoutWavelengthM);
}

}  // namespace ld_creator
