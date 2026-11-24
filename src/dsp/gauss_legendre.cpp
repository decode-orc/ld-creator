/*
 * File:        gauss_legendre.cpp
 * Module:      dsp
 * Purpose:     Gauss–Legendre quadrature rules
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/dsp/gauss_legendre.h"

#include <cmath>

#include "ld_creator/dsp/portable_math.h"

namespace ld_creator {

GaussLegendreRule MakeGaussLegendreRule(std::size_t order) {
  GaussLegendreRule rule;
  rule.nodes.resize(order);
  rule.weights.resize(order);
  const std::size_t n = order;
  constexpr int kNewtonIterations = 100;
  constexpr double kConvergence = 1e-15;
  for (std::size_t i = 0; i < n; ++i) {
    double x = CosTurns((static_cast<double>(i) + 0.75) /
                        (2.0 * static_cast<double>(n) + 1.0));
    double derivative = 0.0;
    for (int iteration = 0; iteration < kNewtonIterations; ++iteration) {
      double p0 = 1.0;
      double p1 = x;
      for (std::size_t k = 2; k <= n; ++k) {
        const double kd = static_cast<double>(k);
        const double p2 = ((2.0 * kd - 1.0) * x * p1 - (kd - 1.0) * p0) / kd;
        p0 = p1;
        p1 = p2;
      }
      derivative = static_cast<double>(n) * (x * p1 - p0) / (x * x - 1.0);
      const double step = p1 / derivative;
      x -= step;
      if (std::abs(step) < kConvergence) {
        break;
      }
    }
    rule.nodes[i] = x;
    rule.weights[i] = 2.0 / ((1.0 - x * x) * derivative * derivative);
  }
  return rule;
}

}  // namespace ld_creator
