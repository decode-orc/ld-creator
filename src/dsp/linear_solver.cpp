/*
 * File:        linear_solver.cpp
 * Module:      dsp
 * Purpose:     Cholesky solution of symmetric positive-definite systems
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/dsp/linear_solver.h"

#include <cassert>
#include <cmath>
#include <stdexcept>

namespace ld_creator {

std::vector<double> SolveSymmetricPositiveDefinite(SquareMatrix matrix,
                                                   std::vector<double> rhs) {
  const std::size_t n = matrix.size;
  assert(rhs.size() == n);
  // Factorise in place: the lower triangle becomes L.
  for (std::size_t column = 0; column < n; ++column) {
    double diagonal = matrix.At(column, column);
    for (std::size_t k = 0; k < column; ++k) {
      diagonal -= matrix.At(column, k) * matrix.At(column, k);
    }
    if (!(diagonal > 0.0)) {
      throw std::runtime_error("matrix is not positive definite");
    }
    const double root = std::sqrt(diagonal);
    matrix.At(column, column) = root;
    for (std::size_t row = column + 1; row < n; ++row) {
      double value = matrix.At(row, column);
      for (std::size_t k = 0; k < column; ++k) {
        value -= matrix.At(row, k) * matrix.At(column, k);
      }
      matrix.At(row, column) = value / root;
    }
  }
  // L·y = b.
  for (std::size_t row = 0; row < n; ++row) {
    double value = rhs[row];
    for (std::size_t k = 0; k < row; ++k) {
      value -= matrix.At(row, k) * rhs[k];
    }
    rhs[row] = value / matrix.At(row, row);
  }
  // Lᵀ·x = y.
  for (std::size_t index = n; index-- > 0;) {
    double value = rhs[index];
    for (std::size_t k = index + 1; k < n; ++k) {
      value -= matrix.At(k, index) * rhs[k];
    }
    rhs[index] = value / matrix.At(index, index);
  }
  return rhs;
}

}  // namespace ld_creator
