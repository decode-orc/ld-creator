/*
 * File:        readout_kernel.cpp
 * Module:      optical_readout
 * Purpose:     S11: line-spread function and edge response of the readout
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/optical_readout/readout_kernel.h"

#include <cmath>
#include <cstddef>

#include "ld_creator/dsp/gauss_legendre.h"
#include "ld_creator/dsp/portable_math.h"
#include "ld_creator/optical_readout/readout_transfer.h"

namespace ld_creator {

namespace {

constexpr std::size_t kLineSpreadPanels = 32;
constexpr std::size_t kLineSpreadOrder = 16;

}  // namespace

ReadoutLineSpread::ReadoutLineSpread(double numerical_aperture,
                                     double rim_intensity)
    : cutoff_per_m_(2.0 * numerical_aperture / kReadoutWavelengthM) {
  const GaussLegendreRule rule = MakeGaussLegendreRule(kLineSpreadOrder);
  const double panel = 1.0 / static_cast<double>(kLineSpreadPanels);
  frequency_.reserve(kLineSpreadPanels * kLineSpreadOrder);
  weight_.reserve(kLineSpreadPanels * kLineSpreadOrder);
  for (std::size_t p = 0; p < kLineSpreadPanels; ++p) {
    for (std::size_t q = 0; q < kLineSpreadOrder; ++q) {
      const double t =
          panel * (static_cast<double>(p) + (0.5 * (rule.nodes[q] + 1.0)));
      const double nu = cutoff_per_m_ * (1.0 - (t * t));
      // dν = fc·2t·dt; the factor 2 of LSF = 2∫…; panel Jacobian ½·panel.
      const double weight = 2.0 * cutoff_per_m_ * 2.0 * t * 0.5 * panel *
                            rule.weights[q] *
                            ReadoutTransfer(nu, numerical_aperture,
                                            rim_intensity, kReadoutWavelengthM);
      frequency_.push_back(nu);
      weight_.push_back(weight);
    }
  }
}

double ReadoutLineSpread::operator()(double u) const {
  double sum = 0.0;
  for (std::size_t i = 0; i < frequency_.size(); ++i) {
    sum += weight_[i] * CosTurns(frequency_[i] * u);
  }
  return sum;
}

EdgeResponseTable MakeReadoutEdgeResponse(double numerical_aperture,
                                          double rim_intensity) {
  const ReadoutLineSpread line_spread(numerical_aperture, rim_intensity);
  const auto kernel = [&line_spread](double u) {
    const double magnitude = std::abs(u);
    if (magnitude <= kReadoutKernelCoreM) {
      return line_spread(u);
    }
    const double into_taper =
        (magnitude - kReadoutKernelCoreM) / kReadoutKernelTaperM;
    if (into_taper >= 1.0) {
      return 0.0;
    }
    // Raised cosine: ½(1 + cos πs), s = 0 … 1.
    return line_spread(u) * 0.5 * (1.0 + CosTurns(0.5 * into_taper));
  };
  return {kernel, kReadoutKernelCoreM + kReadoutKernelTaperM,
          kEdgeTableSpacingM};
}

double PitContrast(double hfmi, double numerical_aperture,
                   double rim_intensity) {
  return hfmi / MaximumHfmi(numerical_aperture, rim_intensity);
}

}  // namespace ld_creator
