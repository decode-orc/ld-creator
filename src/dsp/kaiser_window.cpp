/*
 * File:        kaiser_window.cpp
 * Module:      dsp
 * Purpose:     Kaiser window and the modified Bessel function I0
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/dsp/kaiser_window.h"

#include <cmath>

namespace ld_creator {

double BesselI0(double x) {
  const double half_x_squared = 0.25 * x * x;
  double term = 1.0;
  double sum = 1.0;
  for (int k = 1;; ++k) {
    term *= half_x_squared / (static_cast<double>(k) * static_cast<double>(k));
    const double next = sum + term;
    if (next == sum) {
      break;
    }
    sum = next;
  }
  return sum;
}

double KaiserWindow(double x, double beta) {
  if (x < -1.0 || x > 1.0) {
    return 0.0;
  }
  // std::sqrt is correctly rounded (IEEE 754), so this is host independent.
  return BesselI0(beta * std::sqrt(1.0 - x * x)) / BesselI0(beta);
}

}  // namespace ld_creator
