/*
 * File:        fft.cpp
 * Module:      dsp
 * Purpose:     Deterministic radix-2 fast Fourier transform
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/dsp/fft.h"

#include <cstddef>
#include <stdexcept>
#include <utility>

#include "ld_creator/dsp/portable_math.h"

namespace ld_creator {

void FastFourierTransform(std::vector<ComplexValue>* values, bool inverse) {
  std::vector<ComplexValue>& x = *values;
  const std::size_t n = x.size();
  if (n == 0 || (n & (n - 1)) != 0) {
    throw std::invalid_argument("FFT length must be a power of two");
  }
  for (std::size_t i = 1, j = 0; i < n; ++i) {
    std::size_t bit = n >> 1U;
    for (; (j & bit) != 0; bit >>= 1U) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      std::swap(x[i], x[j]);
    }
  }
  // Twiddles e^(∓2πi·k/N) for k < N/2, each from its exact turn fraction.
  std::vector<ComplexValue> twiddle(n / 2);
  const double sign = inverse ? 1.0 : -1.0;
  for (std::size_t k = 0; k < n / 2; ++k) {
    const SinCosValue value =
        SinCosTurns(static_cast<double>(k) / static_cast<double>(n));
    twiddle[k] = {.re = value.cos, .im = sign * value.sin};
  }
  for (std::size_t length = 2; length <= n; length <<= 1U) {
    const std::size_t half = length / 2;
    const std::size_t stride = n / length;
    for (std::size_t start = 0; start < n; start += length) {
      for (std::size_t k = 0; k < half; ++k) {
        const ComplexValue odd = twiddle[k * stride] * x[start + k + half];
        const ComplexValue even = x[start + k];
        x[start + k] = even + odd;
        x[start + k + half] = even - odd;
      }
    }
  }
}

}  // namespace ld_creator
