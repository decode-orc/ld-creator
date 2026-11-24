/*
 * File:        kaiser_window.h
 * Module:      dsp
 * Purpose:     Kaiser window and the modified Bessel function I0
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

namespace ld_creator {

// The modified Bessel function of the first kind, order 0, by its power
// series Σ ((x/2)^k / k!)², summed until a term no longer changes the sum.
// Every term is positive, so the sum has no cancellation. Pure function;
// O(|x|) terms.
double BesselI0(double x);

// The Kaiser window w(x) = I0(β·√(1 − x²)) / I0(β) for |x| ≤ 1, and 0
// outside (Kaiser, "Nonrecursive digital filter design using the I0-sinh
// window function", 1974). Pure function.
double KaiserWindow(double x, double beta);

}  // namespace ld_creator
