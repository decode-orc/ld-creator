/*
 * File:        gauss_legendre.h
 * Module:      dsp
 * Purpose:     Gauss–Legendre quadrature rules
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstddef>
#include <vector>

namespace ld_creator {

// An n-point Gauss–Legendre rule on [−1, 1]: ∫ f ≈ Σ weights[i]·f(nodes[i]),
// exact for polynomials of degree 2n − 1.
struct GaussLegendreRule {
  std::vector<double> nodes;
  std::vector<double> weights;
};

// Nodes and weights by Newton iteration on the Legendre polynomial from the
// Chebyshev-like starting point cos(π(i + ¾)/(n + ½)) (standard method).
// Uses only + − × ÷ and dsp/portable_math.h, so the rule is the same on
// every host. Pure function, O(n²).
GaussLegendreRule MakeGaussLegendreRule(std::size_t order);

}  // namespace ld_creator
