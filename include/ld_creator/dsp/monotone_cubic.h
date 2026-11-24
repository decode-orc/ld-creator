/*
 * File:        monotone_cubic.h
 * Module:      dsp
 * Purpose:     Shape-preserving piecewise cubic interpolation and its integral
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <vector>

namespace ld_creator {

// A piecewise cubic Hermite interpolant through (x_i, y_i) whose slopes are
// chosen by the Fritsch–Butland rule (F. N. Fritsch and J. Butland, "A method
// for constructing local monotone piecewise cubic interpolants", SIAM J. Sci.
// Stat. Comput. 5, 1984): at an interior point the slope is the weighted
// harmonic mean of the two neighbouring secants, or 0 where they differ in
// sign or either is 0. The slopes at both end points are 0, and the curve is
// held constant outside [x_0, x_last]. So the curve passes through every
// point, is continuous with a continuous slope, and between two points never
// leaves the range of their values.
//
// Thread safety: immutable after construction.
class MonotoneCubic {
 public:
  // `x` strictly ascending, same length as `y`, at least one point.
  MonotoneCubic(std::vector<double> x, std::vector<double> y);

  // The interpolant at `at`. O(log n).
  [[nodiscard]] double Value(double at) const;

  // ∫ from x_0 to `at` of the interpolant (negative for at < x_0, where
  // the curve is the constant y_0). O(n).
  [[nodiscard]] double Integral(double at) const;

 private:
  std::vector<double> x_;
  std::vector<double> y_;
  std::vector<double> slope_;
};

}  // namespace ld_creator
