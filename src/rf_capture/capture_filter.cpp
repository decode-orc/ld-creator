/*
 * File:        capture_filter.cpp
 * Module:      rf_capture
 * Purpose:     S13: the capture device's analogue chain as digital filters
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/rf_capture/capture_filter.h"

#include <cmath>
#include <cstddef>
#include <numbers>

#include "ld_creator/dsp/fft.h"
#include "ld_creator/dsp/kaiser_window.h"
#include "ld_creator/dsp/portable_math.h"
#include "ld_creator/optical_readout/readout_kernel.h"

namespace ld_creator {

namespace {

// DdD RF Amplifier sheet: C401, R402 ∥ R403 (register S13.ac_coupling).
constexpr double kInputCouplingTauS = 500.0 * 100e-9;
// R405, C402 and R406 (register S13.amplifier_feedback).
constexpr double kFeedbackGroundOhm = 200.0;
constexpr double kFeedbackCapacitorF = 100e-9;
constexpr double kFeedbackGainOhm = 1500.0;

// FIR realisation of the DdD filter (register S13.filter_realisation).
constexpr std::size_t kDesignTransformLength = std::size_t{1} << 15U;
constexpr double kTaperStartHz = 36.0e6;
constexpr double kTaperEndHz = 40.0e6;
constexpr double kTruncationFraction = 1e-9;

// The ideal profile's filter (register S13.ideal_filter).
constexpr double kIdealCutoffHz = 17.5e6;
constexpr double kIdealTransitionHz = 5.0e6;
constexpr double kIdealDesignAttenuationDb = 100.0;

ComplexValue Impedance(double frequency_hz, double inductance_h,
                       double capacitance_f) {
  // L ∥ C: jωL / (1 − ω²LC).
  const double omega = kTwoPi * frequency_hz;
  return ComplexValue{.re = 0.0, .im = omega * inductance_h} /
         ComplexValue{
             .re = 1.0 - (omega * omega * inductance_h * capacitance_f),
             .im = 0.0};
}

ComplexValue ShuntAdmittance(double frequency_hz, double capacitance_f) {
  return {.re = 0.0, .im = kTwoPi * frequency_hz * capacitance_f};
}

ComplexValue Parallel(ComplexValue impedance, ComplexValue admittance) {
  // Z ∥ (1/Y) = Z / (1 + Z·Y).
  return impedance /
         (ComplexValue{.re = 1.0, .im = 0.0} + (impedance * admittance));
}

ComplexValue FirstOrder(double frequency_hz, double bs, double b0s, double as,
                        double a0s) {
  const double omega = kTwoPi * frequency_hz;
  return ComplexValue{.re = b0s, .im = bs * omega} /
         ComplexValue{.re = a0s, .im = as * omega};
}

double AmplifierPoleTauS() { return kFeedbackGroundOhm * kFeedbackCapacitorF; }
double AmplifierZeroTauS() {
  return (kFeedbackGroundOhm + kFeedbackGainOhm) * kFeedbackCapacitorF;
}

}  // namespace

ComplexValue DddAntiAliasResponse(double frequency_hz,
                                  const DddFilterComponents& c) {
  if (frequency_hz == 0.0) {
    return {.re = 1.0, .im = 0.0};
  }
  const ComplexValue one{.re = 1.0, .im = 0.0};
  // Impedances looking towards the output from each node; C is loaded
  // only by C407.
  const ComplexValue node_c = one / ShuntAdmittance(frequency_hz, c.c407_f);
  const ComplexValue series_bc = Impedance(frequency_hz, c.l402_h, c.c406_f);
  const ComplexValue node_b =
      Parallel(series_bc + node_c, ShuntAdmittance(frequency_hz, c.c405_f));
  const ComplexValue series_ab = Impedance(frequency_hz, c.l401_h, c.c404_f);
  const ComplexValue node_a =
      Parallel(series_ab + node_b, ShuntAdmittance(frequency_hz, c.c403_f));
  const ComplexValue source{.re = c.r410_ohm, .im = 0.0};
  const ComplexValue v_a = node_a / (source + node_a);
  const ComplexValue v_b = v_a * (node_b / (series_ab + node_b));
  const ComplexValue v_c = v_b * (node_c / (series_bc + node_c));
  return v_c;
}

ComplexValue DddInputCouplingResponse(double frequency_hz) {
  return FirstOrder(frequency_hz, kInputCouplingTauS, 0.0, kInputCouplingTauS,
                    1.0);
}

ComplexValue DddAmplifierResponse(double frequency_hz) {
  const double pole = AmplifierPoleTauS();
  return FirstOrder(frequency_hz, pole, pole / AmplifierZeroTauS(), pole, 1.0);
}

ComplexValue FirFilter::ResponseAt(double frequency_hz, double rate_hz) const {
  ComplexValue sum;
  for (std::size_t k = 0; k < taps.size(); ++k) {
    const double turns =
        -frequency_hz *
        static_cast<double>(first_index + static_cast<std::int64_t>(k)) /
        rate_hz;
    const SinCosValue value = SinCosTurns(turns);
    sum = sum + (taps[k] * ComplexValue{.re = value.cos, .im = value.sin});
  }
  return sum;
}

FirstOrderSection FirstOrderSection::Bilinear(double bs, double b0s, double as,
                                              double a0s, double rate_hz) {
  // s = K·(1 − z⁻¹)/(1 + z⁻¹), K = 2·rate.
  const double k = 2.0 * rate_hz;
  const double den0 = (as * k) + a0s;
  return {.b0 = ((bs * k) + b0s) / den0,
          .b1 = (b0s - (bs * k)) / den0,
          .a1 = (a0s - (as * k)) / den0};
}

ComplexValue FirstOrderSection::ResponseAt(double frequency_hz,
                                           double rate_hz) const {
  const SinCosValue value = SinCosTurns(-frequency_hz / rate_hz);
  const ComplexValue delay{.re = value.cos, .im = value.sin};
  return (ComplexValue{.re = b0, .im = 0.0} + (b1 * delay)) /
         (ComplexValue{.re = 1.0, .im = 0.0} + (a1 * delay));
}

FirFilter DesignDddAntiAliasFir(const DddFilterComponents& components) {
  const std::size_t n = kDesignTransformLength;
  const double spacing = kReadoutRateHz / static_cast<double>(n);
  std::vector<ComplexValue> spectrum(n);
  for (std::size_t k = 0; k <= n / 2; ++k) {
    const double f = static_cast<double>(k) * spacing;
    double taper = 1.0;
    if (f >= kTaperEndHz) {
      taper = 0.0;
    } else if (f > kTaperStartHz) {
      const double s = (f - kTaperStartHz) / (kTaperEndHz - kTaperStartHz);
      taper = SmoothStepDown(s);
    }
    spectrum[k] = taper * DddAntiAliasResponse(f, components);
    if (k > 0 && k < n / 2) {
      spectrum[n - k] = Conjugate(spectrum[k]);
    }
  }
  FastFourierTransform(&spectrum, /*inverse=*/true);
  // h[m] for m = −n/2 … n/2 − 1, stored at index (m + n) mod n.
  std::vector<double> impulse(n);
  double total = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t index = (i + (n / 2)) % n;
    impulse[i] = spectrum[index].re / static_cast<double>(n);
    total += std::abs(impulse[i]);
  }
  // Trim both ends while the trimmed mass stays within the budget.
  const double budget = kTruncationFraction * total;
  std::size_t begin = 0;
  std::size_t end = n;
  double trimmed = 0.0;
  while (begin < end) {
    const double front = std::abs(impulse[begin]);
    const double back = std::abs(impulse[end - 1]);
    const double smaller = front <= back ? front : back;
    if (trimmed + smaller > budget) {
      break;
    }
    trimmed += smaller;
    if (front <= back) {
      ++begin;
    } else {
      --end;
    }
  }
  FirFilter filter;
  filter.taps.assign(impulse.begin() + static_cast<std::ptrdiff_t>(begin),
                     impulse.begin() + static_cast<std::ptrdiff_t>(end));
  filter.first_index =
      static_cast<std::int64_t>(begin) - static_cast<std::int64_t>(n / 2);
  return filter;
}

FirFilter DesignIdealAntiAliasFir() {
  // Kaiser (1974): β = 0.1102·(A − 8.7); length from D = (A − 7.95)/14.36.
  const double beta = 0.1102 * (kIdealDesignAttenuationDb - 8.7);
  const double product = (kIdealDesignAttenuationDb - 7.95) / 14.36;
  auto half = static_cast<std::int64_t>(
      std::ceil(0.5 * product * kReadoutRateHz / kIdealTransitionHz));
  const std::int64_t length = (2 * half) + 1;
  const double cutoff = kIdealCutoffHz / kReadoutRateHz;
  FirFilter filter;
  filter.first_index = -half;
  filter.taps.resize(static_cast<std::size_t>(length));
  for (std::int64_t k = -half; k <= half; ++k) {
    const double m = static_cast<double>(k);
    const double sinc =
        k == 0 ? 2.0 * cutoff : SinTurns(cutoff * m) / (std::numbers::pi * m);
    filter.taps[static_cast<std::size_t>(k + half)] =
        sinc * KaiserWindow(m / static_cast<double>(half), beta);
  }
  return filter;
}

ComplexValue CaptureChain::ResponseAt(double frequency_hz) const {
  ComplexValue response = fir.ResponseAt(frequency_hz, kReadoutRateHz);
  for (const FirstOrderSection& section : sections) {
    response = response * section.ResponseAt(frequency_hz, kReadoutRateHz);
  }
  return response;
}

CaptureChain MakeCaptureChain(CaptureProfile profile) {
  CaptureChain chain;
  if (profile == CaptureProfile::kIdeal) {
    chain.fir = DesignIdealAntiAliasFir();
    return chain;
  }
  chain.sections.push_back(FirstOrderSection::Bilinear(
      kInputCouplingTauS, 0.0, kInputCouplingTauS, 1.0, kReadoutRateHz));
  const double pole = AmplifierPoleTauS();
  chain.sections.push_back(FirstOrderSection::Bilinear(
      pole, pole / AmplifierZeroTauS(), pole, 1.0, kReadoutRateHz));
  chain.fir = DesignDddAntiAliasFir();
  return chain;
}

}  // namespace ld_creator
