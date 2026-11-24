/*
 * File:        complex_value.h
 * Module:      dsp
 * Purpose:     Complex arithmetic written out in + − × ÷
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cmath>

namespace ld_creator {

// A complex number whose operations are spelled out, so that every host
// rounds them the same way. std::complex is not used for values that reach
// the RF: its division and multiplication differ between standard libraries
// (scaling, NaN recovery, FMA), which would break host independence
// (HLD §2.3).
struct ComplexValue {
  double re = 0.0;
  double im = 0.0;
};

inline ComplexValue operator+(ComplexValue a, ComplexValue b) {
  return {.re = a.re + b.re, .im = a.im + b.im};
}
inline ComplexValue operator-(ComplexValue a, ComplexValue b) {
  return {.re = a.re - b.re, .im = a.im - b.im};
}
inline ComplexValue operator*(ComplexValue a, ComplexValue b) {
  return {.re = (a.re * b.re) - (a.im * b.im),
          .im = (a.re * b.im) + (a.im * b.re)};
}
inline ComplexValue operator*(double a, ComplexValue b) {
  return {.re = a * b.re, .im = a * b.im};
}
// Textbook division, (a·b̄)/|b|². The values divided here are filter
// impedances far from overflow, so no scaling is needed.
inline ComplexValue operator/(ComplexValue a, ComplexValue b) {
  const double norm = (b.re * b.re) + (b.im * b.im);
  return {.re = ((a.re * b.re) + (a.im * b.im)) / norm,
          .im = ((a.im * b.re) - (a.re * b.im)) / norm};
}
inline ComplexValue Conjugate(ComplexValue a) {
  return {.re = a.re, .im = -a.im};
}
// |a|, with std::sqrt (correctly rounded).
inline double Magnitude(ComplexValue a) {
  return std::sqrt((a.re * a.re) + (a.im * a.im));
}

}  // namespace ld_creator
