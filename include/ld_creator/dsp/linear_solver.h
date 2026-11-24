/*
 * File:        linear_solver.h
 * Module:      dsp
 * Purpose:     Cholesky solution of symmetric positive-definite systems
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstddef>
#include <vector>

namespace ld_creator {

// A dense square matrix, row major.
struct SquareMatrix {
  std::size_t size = 0;
  std::vector<double> values;  // size × size

  explicit SquareMatrix(std::size_t n) : size(n), values(n * n, 0.0) {}
  double& At(std::size_t row, std::size_t column) {
    return values[(row * size) + column];
  }
  [[nodiscard]] double At(std::size_t row, std::size_t column) const {
    return values[(row * size) + column];
  }
};

// Solves A·x = b for symmetric positive-definite A by Cholesky factorisation
// (A = L·Lᵀ, then two triangular solves), in a fixed operation order so the
// result is the same on every host. Throws std::runtime_error if A is not
// positive definite. O(n³).
std::vector<double> SolveSymmetricPositiveDefinite(SquareMatrix matrix,
                                                   std::vector<double> rhs);

}  // namespace ld_creator
