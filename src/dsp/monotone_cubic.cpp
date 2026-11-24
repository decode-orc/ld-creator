/*
 * File:        monotone_cubic.cpp
 * Module:      dsp
 * Purpose:     Shape-preserving piecewise cubic interpolation and its integral
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/dsp/monotone_cubic.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <utility>

namespace ld_creator {

namespace {

// ∫ of the cubic Hermite segment of width `width` from its start to the
// fraction `u` of the way along, with end values y0, y1 and end slopes m0,
// m1 already scaled by the width (the integral of each basis function times
// the width).
double SegmentIntegral(double y0, double y1, double m0, double m1, double u,
                       double width) {
  const double u2 = u * u;
  const double u3 = u2 * u;
  const double u4 = u3 * u;
  // ∫ h00 = u − u³ + u⁴/2;  ∫ h10 = u²/2 − 2u³/3 + u⁴/4;
  // ∫ h01 = u³ − u⁴/2;      ∫ h11 = −u³/3 + u⁴/4.
  const double i00 = u - u3 + (0.5 * u4);
  const double i10 = (0.5 * u2) - (2.0 / 3.0 * u3) + (0.25 * u4);
  const double i01 = u3 - (0.5 * u4);
  const double i11 = (-1.0 / 3.0 * u3) + (0.25 * u4);
  return width * ((y0 * i00) + (m0 * i10) + (y1 * i01) + (m1 * i11));
}

}  // namespace

MonotoneCubic::MonotoneCubic(std::vector<double> x, std::vector<double> y)
    : x_(std::move(x)), y_(std::move(y)), slope_(x_.size(), 0.0) {
  assert(!x_.empty() && x_.size() == y_.size());
  for (std::size_t i = 1; i + 1 < x_.size(); ++i) {
    const double h_left = x_[i] - x_[i - 1];
    const double h_right = x_[i + 1] - x_[i];
    const double secant_left = (y_[i] - y_[i - 1]) / h_left;
    const double secant_right = (y_[i + 1] - y_[i]) / h_right;
    if (secant_left == 0.0 || secant_right == 0.0 ||
        (secant_left > 0.0) != (secant_right > 0.0)) {
      continue;
    }
    const double w_left = (2.0 * h_right) + h_left;
    const double w_right = h_right + (2.0 * h_left);
    slope_[i] = (w_left + w_right) /
                ((w_left / secant_left) + (w_right / secant_right));
  }
}

double MonotoneCubic::Value(double at) const {
  if (at <= x_.front()) {
    return y_.front();
  }
  if (at >= x_.back()) {
    return y_.back();
  }
  const auto upper = std::upper_bound(x_.begin(), x_.end(), at);
  const auto i = static_cast<std::size_t>(upper - x_.begin()) - 1;
  const double width = x_[i + 1] - x_[i];
  const double u = (at - x_[i]) / width;
  const double u2 = u * u;
  const double u3 = u2 * u;
  const double h00 = (2.0 * u3) - (3.0 * u2) + 1.0;
  const double h10 = u3 - (2.0 * u2) + u;
  const double h01 = (-2.0 * u3) + (3.0 * u2);
  const double h11 = u3 - u2;
  return (h00 * y_[i]) + (h10 * width * slope_[i]) + (h01 * y_[i + 1]) +
         (h11 * width * slope_[i + 1]);
}

double MonotoneCubic::Integral(double at) const {
  if (at <= x_.front()) {
    return (at - x_.front()) * y_.front();
  }
  double total = 0.0;
  for (std::size_t i = 0; i + 1 < x_.size(); ++i) {
    const double width = x_[i + 1] - x_[i];
    const double u = std::min(1.0, (at - x_[i]) / width);
    total += SegmentIntegral(y_[i], y_[i + 1], width * slope_[i],
                             width * slope_[i + 1], u, width);
    if (at <= x_[i + 1]) {
      return total;
    }
  }
  return total + ((at - x_.back()) * y_.back());
}

}  // namespace ld_creator
