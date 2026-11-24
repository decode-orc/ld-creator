/*
 * File:        readout_kernel.h
 * Module:      optical_readout
 * Purpose:     S11: line-spread function and edge response of the readout
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <vector>

#include "ld_creator/dsp/edge_response_table.h"

namespace ld_creator {

// S11 readout rate Fr, 80 MHz (register S11.readout_rate): its Nyquist
// frequency is above fc·v at every radius, and it is twice the capture rate.
inline constexpr double kReadoutRateHz = 80.0e6;

// Window of the readout kernel (S11 note, "Rendering"; register
// S11.kernel_window): the line-spread function is used unchanged within
// ±10 µm of the spot centre and tapered to zero by a raised cosine over the
// next 10 µm.
inline constexpr double kReadoutKernelCoreM = 10.0e-6;
inline constexpr double kReadoutKernelTaperM = 10.0e-6;

// Spacing of the edge-response tables (register S11.kernel_table_spacing):
// 5 nm keeps the Hermite interpolation error below 1e-10 per edge.
inline constexpr double kEdgeTableSpacingM = 5.0e-9;

// The line-spread function of the linear readout model, the inverse Fourier
// transform of the transfer function H (readout_transfer.h):
//   LSF(u) = 2·∫₀^fc H(ν)·cos(2πνu) dν,   fc = 2·NA/λ,
// with u in metres along the track. Computed with ν = fc·(1 − t²), which
// makes the (fc − ν)^(3/2) behaviour of H at the cut-off smooth in t, and
// 32 panels of 16-point Gauss–Legendre quadrature in t. H is evaluated once,
// at the nodes. Immutable after construction; safe to share between threads.
class ReadoutLineSpread {
 public:
  ReadoutLineSpread(double numerical_aperture, double rim_intensity);

  // LSF(u) in 1/m. O(512).
  [[nodiscard]] double operator()(double u) const;

  // fc in cycles per metre.
  [[nodiscard]] double CutoffPerM() const { return cutoff_per_m_; }

 private:
  double cutoff_per_m_;
  std::vector<double> frequency_;
  std::vector<double> weight_;
};

// The readout kernel: LSF·w, with w = 1 for |u| ≤ kReadoutKernelCoreM and a
// raised-cosine taper to 0 at kReadoutKernelCoreM + kReadoutKernelTaperM,
// as an edge-response table at kEdgeTableSpacingM (S11 note, "Rendering").
EdgeResponseTable MakeReadoutEdgeResponse(double numerical_aperture,
                                          double rim_intensity);

// The pit contrast m that gives `hfmi` with these optics (S11 note,
// "Setting m from the HFMI"): m = hfmi / ((4/π)·H(802 pits/mm)).
double PitContrast(double hfmi, double numerical_aperture,
                   double rim_intensity);

}  // namespace ld_creator
