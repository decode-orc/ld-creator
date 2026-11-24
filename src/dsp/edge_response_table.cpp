/*
 * File:        edge_response_table.cpp
 * Module:      dsp
 * Purpose:     Tabulated edge response of a finite, even line-spread kernel
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/dsp/edge_response_table.h"

#include <cmath>
#include <stdexcept>

#include "ld_creator/dsp/gauss_legendre.h"
#include "ld_creator/dsp/portable_math.h"

namespace ld_creator {

namespace {

constexpr std::size_t kIntervalQuadratureOrder = 5;

}  // namespace

EdgeResponseTable::EdgeResponseTable(
    const std::function<double(double)>& kernel, double half_width,
    double spacing)
    : half_width_(half_width), spacing_(spacing) {
  if (!(half_width > 0.0) || !(spacing > 0.0)) {
    throw std::invalid_argument("edge response table needs W > 0 and h > 0");
  }
  const auto per_side =
      static_cast<std::size_t>(std::llround(half_width / spacing));
  if (per_side < 1 || std::abs((static_cast<double>(per_side) * spacing) -
                               half_width) > 1e-9 * half_width) {
    throw std::invalid_argument(
        "edge response table: W must be a whole number of spacings");
  }
  inverse_spacing_ = 1.0 / spacing_;
  last_interval_ = 2 * per_side;

  // Cumulative integral from 0 to node i, i = 0 … per_side.
  const GaussLegendreRule rule =
      MakeGaussLegendreRule(kIntervalQuadratureOrder);
  std::vector<double> cumulative(per_side + 1, 0.0);
  std::vector<double> node_slope(per_side + 1, 0.0);
  for (std::size_t i = 0; i <= per_side; ++i) {
    node_slope[i] = kernel(static_cast<double>(i) * spacing_);
  }
  for (std::size_t i = 0; i < per_side; ++i) {
    const double left = static_cast<double>(i) * spacing_;
    double sum = 0.0;
    for (std::size_t q = 0; q < kIntervalQuadratureOrder; ++q) {
      sum += rule.weights[q] *
             kernel(left + (0.5 * spacing_ * (rule.nodes[q] + 1.0)));
    }
    cumulative[i + 1] = cumulative[i] + (0.5 * spacing_ * sum);
  }
  mass_ = 2.0 * cumulative[per_side];
  const double half_mass = 0.5 * mass_;

  // Node j is at u = −W + j·h; node per_side is u = 0.
  value_.assign(last_interval_ + 1, 0.0);
  slope_.assign(last_interval_ + 1, 0.0);
  for (std::size_t i = 0; i <= per_side; ++i) {
    value_[per_side + i] = half_mass + cumulative[i];
    value_[per_side - i] = half_mass - cumulative[i];
    slope_[per_side + i] = node_slope[i];
    slope_[per_side - i] = node_slope[i];
  }
}

double EdgeResponseTable::Transfer(double frequency) const {
  const std::size_t per_side = last_interval_ / 2;
  // 2·∫₀^W k(u) cos(2πνu) du by the trapezoidal rule; k(W) = 0.
  double sum = 0.5 * slope_[per_side];
  for (std::size_t i = 1; i < per_side; ++i) {
    sum += slope_[per_side + i] *
           CosTurns(frequency * static_cast<double>(i) * spacing_);
  }
  return 2.0 * spacing_ * sum;
}

}  // namespace ld_creator
