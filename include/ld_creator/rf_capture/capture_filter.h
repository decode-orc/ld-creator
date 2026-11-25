/*
 * File:        capture_filter.h
 * Module:      rf_capture
 * Purpose:     S13: the capture device's analogue chain as digital filters
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>
#include <vector>

#include "ld_creator/dsp/complex_value.h"
#include "ld_creator/project/project_config.h"

namespace ld_creator {

// DomesdayDuplicator anti-alias filter (RF Amplifier sheet, rev 3.0.2).
// Topology from the board's netlist (register S13.filter_topology; S13
// note, "Filter model"): the OPA690 output drives R410 into node A; C403
// shunts A; L401 ∥ C404 joins A to B; C405 shunts B; L402 ∥ C406 joins B
// to C; C407 shunts C; R411 takes C to the ADS825 input, which is unloaded
// (its current is neglected), so the output is the voltage at C. The
// defaults are the schematic's nominal values (register
// S13.filter_components).
struct DddFilterComponents {
  double r410_ohm = 47.0;
  double c403_f = 330e-12;
  double l401_h = 680e-9;
  double c404_f = 33e-12;
  double c405_f = 470e-12;
  double l402_h = 560e-9;
  double c406_f = 100e-12;
  double c407_f = 270e-12;
};

// Effective component values fitted to the measured board (register
// S13.filter_fit): in the netlist's topology, with the amplifier bandwidth
// below, they reproduce the DdD spreadsheet's sheet 1 magnitude within
// 0.18 dB to 17.8 MHz and its sheet 2 phase within 6° to 16 MHz. They
// describe the board's response, not its parts: the nominal values within
// their tolerances do not reach the measurement (S13 note).
DddFilterComponents DddFittedFilterComponents();

// The LC filter's voltage transfer from the amplifier output to node C at
// `frequency_hz`, by ladder analysis. Pure function.
ComplexValue DddAntiAliasResponse(double frequency_hz,
                                  const DddFilterComponents& components = {});

// The OPA690's closed-loop bandwidth as a single pole at GBP / noise gain:
// 250 MHz (OPA690ID, VS = 5 V, TI SBOS223H table 6.8) / 8.5 = 29.4 MHz
// (register S13.amplifier_bandwidth). Pure function.
ComplexValue DddAmplifierBandwidthResponse(double frequency_hz);

// The DdD's high-frequency response, which the FIR realises: the
// amplifier's bandwidth times the LC filter. Pure function.
ComplexValue DddHighFrequencyResponse(double frequency_hz,
                                      const DddFilterComponents& components);

// First-order sections of the DdD input (register S13.ac_coupling,
// S13.amplifier_feedback), as continuous-time responses:
//   input coupling: C401 = 100 nF into R402 ∥ R403 = 500 Ω, sτ/(1 + sτ);
//   amplifier: gain 1 + Z_A/(R405 + 1/(s·C402)) with R405 = 200 Ω,
//     C402 = 100 nF and Z_A = R406 = 1.5 kΩ (gain switch "1000", the
//     setting of the measured response), normalised to unity gain at high
//     frequency.
ComplexValue DddInputCouplingResponse(double frequency_hz);
ComplexValue DddAmplifierResponse(double frequency_hz);

// y[n] = Σₖ taps[k]·x[n − first_index − k].
struct FirFilter {
  std::vector<double> taps;
  std::int64_t first_index = 0;

  // Σₖ taps[k]·e^(−2πi·f·(first_index + k)/rate). O(taps).
  [[nodiscard]] ComplexValue ResponseAt(double frequency_hz,
                                        double rate_hz) const;
};

// y[n] = b0·x[n] + b1·x[n−1] − a1·y[n−1]: the bilinear transform of
// (bs·s + b0s)/(as·s + a0s) at `rate_hz`. The corners realised here are
// below 10 kHz at 80 MHz. The transform's frequency warping maps f to
// (Fr/π)·tan(πf/Fr), so near Nyquist it drops what remains of the
// sections' phase tail, 1/(2πfτ): at most 1.2e-4 (−78 dB) at any frequency
// (unit tests).
struct FirstOrderSection {
  double b0 = 1.0;
  double b1 = 0.0;
  double a1 = 0.0;

  static FirstOrderSection Bilinear(double bs, double b0s, double as,
                                    double a0s, double rate_hz);
  [[nodiscard]] ComplexValue ResponseAt(double frequency_hz,
                                        double rate_hz) const;
};

// The DdD high-frequency response (amplifier bandwidth and LC filter, with
// the fitted components by default) as an FIR at the readout rate Fr (S13 note,
// "Realisation"): the continuous response is sampled at 2^15 frequencies
// across Fr, tapered to zero between 36 and 40 MHz by SmoothStepDown (D9
// has no content above 35.7 MHz, so the taper changes nothing that
// reaches the filter; it makes the periodic response smooth, so the impulse
// response decays fast), and inverse-transformed. Taps are kept over the
// shortest span outside which Σ|h| is below 1e-9 of the total (register
// S13.filter_truncation).
FirFilter DesignDddAntiAliasFir(
    const DddFilterComponents& components = DddFittedFilterComponents());

// The `ideal` profile's anti-alias low-pass at Fr (register
// S13.ideal_filter): Kaiser-windowed sinc, cut-off 17.5 MHz, transition
// 15–20 MHz, A = 100 dB, symmetric about tap 0 (zero phase). Meets
// passband ripple < 0.01 dB to 15 MHz and ≥ 98 dB attenuation from 20 MHz
// (unit tests).
FirFilter DesignIdealAntiAliasFir();

// The whole analogue chain of a capture profile at Fr, in the order
// applied: first-order sections, then the FIR.
struct CaptureChain {
  std::vector<FirstOrderSection> sections;
  FirFilter fir;

  [[nodiscard]] ComplexValue ResponseAt(double frequency_hz) const;
};

CaptureChain MakeCaptureChain(CaptureProfile profile);

}  // namespace ld_creator
