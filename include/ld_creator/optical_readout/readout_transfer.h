/*
 * File:        readout_transfer.h
 * Module:      optical_readout
 * Purpose:     Linear readout transfer function of the reference optics
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

namespace ld_creator {

// IEC 60857-1986 11.1.1: reference wavelength 632.8 nm (register
// S11.wavelength).
inline constexpr double kReadoutWavelengthM = 632.8e-9;

// IEC 60857-1986 Amendment 2 Figure 18: HFMI is specified at 802 pits/mm
// (register S11.hfmi_reference_frequency).
inline constexpr double kHfmiReferenceFrequencyPerM = 802.0e3;

// Transfer function H of the linear readout model (S11 note, "Detection and
// transfer function"): the normalised autocorrelation of a pupil with
// Gaussian amplitude exp(−β·ρ²/2), β = −ln(rim_intensity), at spatial
// frequency `frequency_per_m` (cycles per metre along the track).
//
// Method: with ν = frequency·λ/NA the pupils of radius 1 at 0 and (ν, 0)
// overlap in a lens that is symmetric about x = ν/2. The Gaussian factorises,
// so the integral across the track is exp(−β·y²) integrated analytically
// (an erf); the remaining integral along x over [ν/2, 1] is taken with the
// substitution x = cos θ, which removes the square-root end-point behaviour,
// and 64-point Gauss–Legendre quadrature. The integrand is then smooth and
// the quadrature error is below 1e-12 (checked against the uniform-pupil
// closed form in the unit tests).
//
// Every transcendental function is from dsp/portable_math.h, so the value
// is the same on every host (HLD §2.3).
//
// Returns 1 at frequency 0 and 0 at or above the cut-off 2·NA/λ. Pure
// function, safe from any thread. Complexity O(1).
double ReadoutTransfer(double frequency_per_m, double numerical_aperture,
                       double rim_intensity, double wavelength_m);

// The largest HFMI the linear readout can give, at pit contrast m = 1
// (S11 note, "Setting m from the HFMI"): (4/π)·H(802 pits/mm).
double MaximumHfmi(double numerical_aperture, double rim_intensity);

}  // namespace ld_creator
