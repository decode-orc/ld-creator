/*
 * File:        fft.h
 * Module:      dsp
 * Purpose:     Deterministic radix-2 fast Fourier transform
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <vector>

#include "ld_creator/dsp/complex_value.h"

namespace ld_creator {

// In-place transform X[k] = Σₙ x[n]·e^(∓2πi·kn/N) of a power-of-two length
// N: the minus sign for the forward transform, plus for the inverse (which
// is not scaled by 1/N). Iterative Cooley–Tukey with bit-reversed input;
// twiddle factors from dsp/portable_math.h, so the result is the same on
// every host. Throws std::invalid_argument if N is not a power of two.
// O(N log N).
void FastFourierTransform(std::vector<ComplexValue>* values, bool inverse);

}  // namespace ld_creator
