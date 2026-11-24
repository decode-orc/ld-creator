/*
 * File:        edge_response_table.h
 * Module:      dsp
 * Purpose:     Tabulated edge response of a finite, even line-spread kernel
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstddef>
#include <functional>
#include <vector>

namespace ld_creator {

// The edge response E(u) = ∫_{−W}^{u} k(t) dt of a kernel k that is even and
// zero outside [−W, W], tabulated at spacing h with its slope k, and read
// back by cubic Hermite interpolation. E = 0 for u ≤ −W and E = M (the
// kernel's mass) for u ≥ W.
//
// Construction: on each table interval the integral is a 5-point
// Gauss–Legendre sum (exact for degree 9); values are accumulated from u = 0
// outwards and mirrored, E(−u) = M − E(u), so E(−W) = 0 exactly.
// Interpolation error: at most h⁴·max|k‴|/384 (Hermite remainder); the
// owners of each table state h and check the bound in their unit tests.
//
// Immutable after construction; safe to share between threads.
class EdgeResponseTable {
 public:
  // `kernel` is called (2 + 5)·N + 1 times for N = W/h intervals per side.
  EdgeResponseTable(const std::function<double(double)>& kernel,
                    double half_width, double spacing);

  // E(u). O(1).
  [[nodiscard]] double operator()(double u) const {
    if (u <= -half_width_) {
      return 0.0;
    }
    if (u >= half_width_) {
      return mass_;
    }
    const double position = (u + half_width_) * inverse_spacing_;
    std::size_t index = static_cast<std::size_t>(position);
    if (index >= last_interval_) {
      index = last_interval_ - 1;
    }
    const double f = position - static_cast<double>(index);
    const double g = 1.0 - f;
    // Hermite basis on [0, 1].
    const double h00 = (1.0 + 2.0 * f) * g * g;
    const double h10 = f * g * g;
    const double h01 = f * f * (3.0 - 2.0 * f);
    const double h11 = -f * f * g;
    return (h00 * value_[index]) + (h01 * value_[index + 1]) +
           (spacing_ * ((h10 * slope_[index]) + (h11 * slope_[index + 1])));
  }

  [[nodiscard]] double HalfWidth() const { return half_width_; }
  [[nodiscard]] double Spacing() const { return spacing_; }
  // M = E(W) = ∫ k.
  [[nodiscard]] double Mass() const { return mass_; }

  // The kernel's Fourier transform ∫ k(u)·cos(2πνu) du at `frequency`
  // (cycles per unit of u), by the trapezoidal rule on the table nodes. k
  // and k′ vanish at ±W and k is even, so the rule is accurate to far below
  // the interpolation bound (unit tests). O(N).
  [[nodiscard]] double Transfer(double frequency) const;

 private:
  double half_width_;
  double spacing_;
  double inverse_spacing_;
  std::size_t last_interval_;
  double mass_ = 0.0;
  std::vector<double> value_;
  std::vector<double> slope_;
};

}  // namespace ld_creator
