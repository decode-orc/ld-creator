/*
 * File:        test_rf_capture.cpp
 * Module:      tests
 * Purpose:     Unit tests for S13, the capture device model
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numbers>
#include <stdexcept>
#include <vector>

#include "ld_creator/optical_readout/optical_readout.h"
#include "ld_creator/optical_readout/readout_kernel.h"
#include "ld_creator/optical_readout/readout_transfer.h"
#include "ld_creator/pipeline/disc_run.h"
#include "ld_creator/player_front_end/player_front_end.h"
#include "ld_creator/rf_capture/capture_filter.h"
#include "ld_creator/rf_capture/rf_capture.h"
#include "pit_train.h"
#include "sinusoid_fit.h"

namespace ld_creator {
namespace {

double Db(double ratio) { return 20.0 * std::log10(ratio); }

// DdD "Gain and filter calculations" sheet 1: frequency (Hz) and peak level
// (dB relative to the ADC's maximum) for 200 mV p-p in at gain switch 1000.
struct MeasuredPoint {
  double frequency_hz;
  double level_db;
};
constexpr std::array<MeasuredPoint, 41> kSheet1 = {{
    {10000, -3.9569},     {12116, -3.2169},     {14678, -2.6693},
    {17783, -2.2638},     {21545, -1.9809},     {26102, -1.7482},
    {31623, -1.6039},     {38312, -1.5024},     {46416, -1.4222},
    {56235, -1.3891},     {68130, -1.3488},     {82541, -1.3290},
    {100000, -1.3231},    {121153, -1.3097},    {146780, -1.3094},
    {177828, -1.3037},    {215444, -1.3035},    {261016, -1.3037},
    {316228, -1.3090},    {383119, -1.3238},    {464159, -1.3289},
    {562342, -1.3437},    {681293, -1.3681},    {825405, -1.4233},
    {1000000, -1.4676},   {1211528, -1.5654},   {1467800, -1.6678},
    {1778280, -1.8342},   {2154435, -2.0469},   {2610158, -2.3293},
    {3162278, -2.6237},   {3831187, -2.9747},   {4641589, -3.2669},
    {5623414, -3.3407},   {6812921, -3.0944},   {8254042, -2.6230},
    {10000000, -3.0480},  {12115277, -4.7400},  {14677993, -4.0375},
    {17782795, -29.7295}, {21544347, -43.5214},
}};

// The model's level for the sheet's conditions: 200 mV p-p × gain 8.5 into
// a 2 V p-p window, times the chain's response.
double ModelDb(double frequency_hz, const DddFilterComponents& components) {
  const double gain = 1.0 + (1500.0 / 200.0);
  return Db(0.2 * gain / 2.0) +
         Db(Magnitude(DddInputCouplingResponse(frequency_hz) *
                      DddAmplifierResponse(frequency_hz) *
                      DddAntiAliasResponse(frequency_hz, components)));
}

// DdD "Gain and filter calculations" sheet 2: phase lag (degrees, wrapped
// to 0-360) at 0.25, 0.5, … 16 MHz, same conditions as sheet 1.
constexpr std::array<double, 64> kSheet2LagDeg = {{
    3.696, 10.99, 15.57, 23.31, 25.99, 33.62, 35.95, 38.96, 46.43, 51.31, 55.13,
    59.42, 65.06, 67.43, 73.68, 73.81, 81.44, 85.4,  89.18, 89.44, 95.54, 99.01,
    105.5, 109.2, 113.7, 114.2, 121.1, 125.4, 130.9, 135,   140,   146.6, 153.6,
    162.4, 165.5, 172.9, 176.7, 188.8, 192,   199.7, 205.6, 214.8, 218.7, 226.2,
    233.7, 237.7, 241,   249.9, 254.2, 261.8, 263.9, 269.3, 281.3, 289,   302,
    330,   357.3, 16.3,  36.6,  48.96, 60.41, 69.93, 72.3,  81.57,
}};
constexpr double kSheet2StepHz = 0.25e6;

// The fitted chain (register S13.filter_fit) at the sheets' conditions:
// first-order sections, amplifier bandwidth and the fitted LC filter.
ComplexValue FittedChain(double frequency_hz) {
  return DddInputCouplingResponse(frequency_hz) *
         DddAmplifierResponse(frequency_hz) *
         DddHighFrequencyResponse(frequency_hz, DddFittedFilterComponents());
}

// The span of the model over every ±5 % corner of the five capacitors.
std::pair<double, double> ToleranceSpan(double frequency_hz) {
  double low = 1e9;
  double high = -1e9;
  for (unsigned corner = 0; corner < 32; ++corner) {
    DddFilterComponents c;
    const auto scale = [corner](unsigned bit) {
      return ((corner >> bit) & 1U) != 0 ? 1.05 : 0.95;
    };
    c.c403_f *= scale(0);
    c.c404_f *= scale(1);
    c.c405_f *= scale(2);
    c.c406_f *= scale(3);
    c.c407_f *= scale(4);
    const double level = ModelDb(frequency_hz, c);
    low = std::min(low, level);
    high = std::max(high, level);
  }
  return {low, high};
}

// Allowance for the sheet's measurement and the amplifier's gain
// resistors, which the capacitor corners do not cover (S13 note).
constexpr double kMeasurementAllowanceDb = 0.2;

// Plan Task 5.3 acceptance, where it holds: up to 5.6 MHz and in the
// stop-band edge (14.7, 17.8 MHz) the measured response lies within the
// span the 5 % capacitor tolerance gives.
TEST(DddCaptureFilterTest, MatchesTheMeasuredResponseWithinComponentTolerance) {
  for (const MeasuredPoint& point : kSheet1) {
    const double f = point.frequency_hz;
    const bool covered = f <= 5.7e6 || (f > 14e6 && f < 18e6);
    if (!covered) {
      continue;
    }
    const auto [low, high] = ToleranceSpan(f);
    EXPECT_GE(point.level_db, low - kMeasurementAllowanceDb) << f;
    EXPECT_LE(point.level_db, high + kMeasurementAllowanceDb) << f;
  }
}

// The disagreement that led to the fit (S13 note): from 6.8 to 12.1 MHz
// the measured response is up to 2.9 dB below the nominal model,
// and at 21.5 MHz the measured −43.5 dB is the instrument's floor, not the
// model's notch. Guards the recorded figure.
TEST(DddCaptureFilterTest, KnownDeviationFromTheMeasurementIsAsRecorded) {
  const DddFilterComponents nominal;
  double worst = 0.0;
  for (const MeasuredPoint& point : kSheet1) {
    if (point.frequency_hz > 6e6 && point.frequency_hz < 13e6) {
      worst = std::min(worst,
                       point.level_db - ModelDb(point.frequency_hz, nominal));
    }
  }
  EXPECT_LT(worst, -2.7);
  EXPECT_GT(worst, -3.0);
}

// Register S13.filter_fit: the fitted chain reproduces sheet 1 within
// 0.18 dB to 17.8 MHz, once the fit's gain offset (+0.114 dB, the gain
// resistors; absorbed by the level setting) is removed, and sheet 2 within
// 6° to 16 MHz, once the fit's 3.66 ns measurement delay is removed.
TEST(DddCaptureFilterTest, FittedChainReproducesTheMeasuredResponse) {
  constexpr double kGainOffsetDb = 0.114;
  constexpr double kMeasurementDelayS = 3.66e-9;
  const double sheet_level = Db(0.2 * (1.0 + (1500.0 / 200.0)) / 2.0);
  for (const MeasuredPoint& point : kSheet1) {
    if (point.frequency_hz > 18e6) {
      continue;  // 21.5 MHz: the instrument's floor
    }
    const double model = sheet_level + kGainOffsetDb +
                         Db(Magnitude(FittedChain(point.frequency_hz)));
    EXPECT_NEAR(point.level_db, model, 0.18) << point.frequency_hz;
  }
  double previous_measured = 0.0;
  double previous_model = 0.0;
  double unwrap_measured = 0.0;
  double unwrap_model = 0.0;
  for (std::size_t i = 0; i < kSheet2LagDeg.size(); ++i) {
    const double f = kSheet2StepHz * static_cast<double>(i + 1);
    const ComplexValue response = FittedChain(f);
    const double model_lag =
        -std::atan2(response.im, response.re) * 180.0 / std::numbers::pi;
    // Unwrap both lags against their previous point.
    unwrap_measured +=
        std::remainder(kSheet2LagDeg[i] - previous_measured, 360.0);
    unwrap_model += std::remainder(model_lag - previous_model, 360.0);
    previous_measured = kSheet2LagDeg[i];
    previous_model = model_lag;
    const double delay_lag = 360.0 * f * kMeasurementDelayS;
    EXPECT_NEAR(unwrap_measured, unwrap_model + delay_lag, 6.0) << f;
  }
}

TEST(DddCaptureFilterTest, FirRealisesTheContinuousFilterInTheReadoutBand) {
  const FirFilter fir = DesignDddAntiAliasFir();
  for (double f = 0.0; f <= 35.7e6; f += 0.1e6) {
    const ComplexValue realised = fir.ResponseAt(f, kReadoutRateHz);
    const ComplexValue exact =
        DddHighFrequencyResponse(f, DddFittedFilterComponents());
    EXPECT_NEAR(realised.re, exact.re, 1e-8) << f;
    EXPECT_NEAR(realised.im, exact.im, 1e-8) << f;
  }
}

TEST(DddCaptureFilterTest, FirstOrderSectionsMatchTheirContinuousResponse) {
  const CaptureChain chain =
      MakeCaptureChain(CaptureProfile::kDomesdayDuplicator);
  ASSERT_EQ(chain.sections.size(), 2U);
  for (double f = 100.0; f < 40e6; f *= 1.5) {
    const ComplexValue coupling =
        chain.sections[0].ResponseAt(f, kReadoutRateHz);
    const ComplexValue amplifier =
        chain.sections[1].ResponseAt(f, kReadoutRateHz);
    EXPECT_LT(Magnitude(coupling - DddInputCouplingResponse(f)), 1.2e-4) << f;
    EXPECT_LT(Magnitude(amplifier - DddAmplifierResponse(f)), 1.2e-4) << f;
  }
}

// Plan Task 5.3 acceptance: above 20 MHz each profile is down by its
// documented amount (S13 note): DdD (fitted chain) at least 41.1 dB below
// its 1 MHz level, ideal at least 98 dB.
TEST(CaptureChainTest, AttenuatesAboveTwentyMegahertzAsDocumented) {
  const CaptureChain ddd =
      MakeCaptureChain(CaptureProfile::kDomesdayDuplicator);
  const double reference = Magnitude(ddd.ResponseAt(1e6));
  double ddd_worst = -1e9;
  for (double f = 20e6; f <= 40e6; f += 0.05e6) {
    ddd_worst =
        std::max(ddd_worst, Db(Magnitude(ddd.ResponseAt(f)) / reference));
  }
  EXPECT_LT(ddd_worst, -41.1);
  EXPECT_GT(ddd_worst, -41.2);

  const CaptureChain ideal = MakeCaptureChain(CaptureProfile::kIdeal);
  for (double f = 20e6; f <= 40e6; f += 0.05e6) {
    EXPECT_LT(Db(Magnitude(ideal.ResponseAt(f))), -98.0) << f;
  }
}

TEST(CaptureChainTest, IdealFilterIsFlatAndZeroPhaseInItsPassband) {
  const CaptureChain ideal = MakeCaptureChain(CaptureProfile::kIdeal);
  for (double f = 0.0; f <= 15e6; f += 0.05e6) {
    const ComplexValue response = ideal.ResponseAt(f);
    EXPECT_LT(std::abs(Db(Magnitude(response))), 0.01) << f;
    EXPECT_NEAR(response.im, 0.0, 1e-12) << f;
  }
}

// A 50 %-duty pit train at carrier `frequency_hz`, read by S11 (and the
// inverse equaliser if asked) at `speed`, captured by S13 with its level
// referenced to `blanking_hz`.
struct Capture {
  std::vector<std::int32_t> codes;
  std::int64_t clipped;
  std::int64_t readout_samples;
  double lowest;
  double highest;
};

// One pass of a pit train at `frequency_hz` through S11 (and S12's
// equaliser when `equalise`) and S13 at `level`.
Capture CapturePass(CaptureProfile profile, bool equalise, double frequency_hz,
                    double speed, std::size_t edges_per_block,
                    const std::function<CaptureLevel(double)>& level_for) {
  constexpr double kNa = 0.40;
  constexpr double kRim = 0.23;
  const double length = speed * 120e-6;
  const test::ConstantSpeedTrajectory spot(speed);
  const ReadoutExtent extent{
      .sample_count = static_cast<std::int64_t>(120e-6 * kReadoutRateHz),
      .end_arc_length_m = length};
  LinearOpticalReadout readout({.numerical_aperture = kNa,
                                .rim_intensity = kRim,
                                .hfmi = MaximumHfmi(kNa, kRim)},
                               &spot, extent);
  InverseMtfEqualiser equaliser(kNa, readout.Contrast(), &spot, extent);
  ReferenceFrontEnd reference;
  IPlayerFrontEnd* front_end =
      equalise ? static_cast<IPlayerFrontEnd*>(&equaliser) : &reference;
  const EdgeResponseTable* transfer =
      equalise ? &equaliser.Response() : &readout.Response();
  RfCapture capture(profile, MakeCaptureChain(profile),
                    level_for(readout.Contrast()));
  Capture result{};
  ReadoutBlock d8;
  PhotocurrentBlock d9;
  RfCodeBlock d10;
  const auto pass = [&](bool last) {
    if (last) {
      front_end->Finish(&d9);
    }
    result.readout_samples += static_cast<std::int64_t>(d9.level.size());
    capture.Process(d9, &d10);
    EXPECT_EQ(d10.first_sample, static_cast<std::int64_t>(result.codes.size()));
    result.codes.insert(result.codes.end(), d10.codes.begin(), d10.codes.end());
  };
  for (const ScanBlock& block :
       test::PitTrain(speed / frequency_hz, length, speed, edges_per_block)) {
    readout.Process(block, &d8);
    front_end->Process(d8, &d9);
    pass(false);
  }
  readout.Finish(&d8);
  front_end->Process(d8, &d9);
  pass(false);
  pass(true);
  capture.Finish(&d10);
  result.codes.insert(result.codes.end(), d10.codes.begin(), d10.codes.end());
  result.clipped = capture.ClippedSamples();
  result.lowest = capture.LowestSampled();
  result.highest = capture.HighestSampled();
  return result;
}

// The S13 level rule, as a disc run applies it: a measuring pass at unit
// scale, then the capture at the level its extremes give.
Capture CaptureCarrier(CaptureProfile profile, bool equalise,
                       double frequency_hz, double speed,
                       std::size_t edges_per_block) {
  const Capture measured =
      CapturePass(profile, equalise, frequency_hz, speed, edges_per_block,
                  [](double contrast) {
                    return MeasuringCaptureLevel(CaptureMean(contrast));
                  });
  const RfCaptureConfig config{.profile = profile, .level_fraction = 0.75};
  return CapturePass(profile, equalise, frequency_hz, speed, edges_per_block,
                     [&](double contrast) {
                       return LevelFromExtremes(config, CaptureMean(contrast),
                                                measured.lowest,
                                                measured.highest);
                     });
}

constexpr double kInnerCavSpeed = 10.3577;
constexpr double kOuterCavSpeed = 27.3066;
constexpr double kClvSpeed = 11.0;

TEST(RfCaptureLevelTest, ScalesTheMeasuredPeakToPeakToTheLevelFraction) {
  const RfCaptureConfig config{.profile = CaptureProfile::kDomesdayDuplicator,
                               .level_fraction = 0.75};
  const CaptureLevel level = LevelFromExtremes(config, 0.6, -0.2, 0.3);
  EXPECT_DOUBLE_EQ(level.mean, 0.6);
  EXPECT_DOUBLE_EQ(level.peak_to_peak, 0.5);
  EXPECT_DOUBLE_EQ(level.scale, 3.0);  // 0.5 → 1.5 = 2 · 0.75
  EXPECT_THROW((void)LevelFromExtremes(config, 0.6, 0.3, 0.3),
               std::invalid_argument);
  EXPECT_DOUBLE_EQ(CaptureMean(0.4), 0.8);
  EXPECT_DOUBLE_EQ(MeasuringCaptureLevel(0.8).scale, 1.0);
}

// The level rule (S13 note, "Level"): the captured RF's peak-to-peak is
// level_fraction of full scale. For a carrier the peak-to-peak is the
// sinusoid's. 8.1 MHz is not locked to the 40 MHz sampling, so over the
// record the samples reach the waveform's peaks (a locked 8 MHz carrier
// would be sampled at fixed phases, below them); 400 samples are exactly
// 81 periods, so the fit is over whole periods. At 55 mm the harmonics are
// beyond the optical cut-off.
TEST(RfCaptureTest, CarrierPeakToPeakFillsTheLevelFractionOfFullScale) {
  for (const CaptureProfile profile :
       {CaptureProfile::kDomesdayDuplicator, CaptureProfile::kIdeal}) {
    for (const bool equalise : {false, true}) {
      const Capture capture =
          CaptureCarrier(profile, equalise, 8.1e6, kInnerCavSpeed, 400);
      // Skip 25 µs at each end (filter start-up and the record's edges).
      const std::size_t skip = 1000;
      const std::size_t count =
          ((capture.codes.size() - (2 * skip)) / 400) * 400;
      double mean = 0.0;
      for (std::size_t n = skip; n < skip + count; ++n) {
        mean += capture.codes[n];
      }
      mean /= static_cast<double>(count);
      std::vector<double> values;
      std::vector<double> times;
      for (std::size_t n = skip; n < skip + count; ++n) {
        values.push_back(capture.codes[n] - mean);
        times.push_back(static_cast<double>(n) / 40e6);
      }
      const double amplitude =
          test::FitSinusoid(values, times, 8.1e6).amplitude;
      const double half_scale =
          profile == CaptureProfile::kIdeal ? 32768.0 : 512.0;
      EXPECT_NEAR(amplitude / half_scale, 0.75, 0.5 / half_scale + 2e-4)
          << static_cast<int>(profile) << equalise;
      EXPECT_EQ(capture.clipped, 0);
    }
  }
}

// Plan Task 5.3 acceptance: no clipping at the default level across the
// carrier's range (sync tip 7.6 MHz, white 9.3 MHz, pre-emphasis overshoot
// 12.15 MHz) at both CAV radii and on CLV: the measured level leaves 25 %
// of the window as headroom. 12.15 MHz alone is tested only at 145 mm: at
// 55 mm and on CLV it lies next to the optical cut-off, where it is so
// small that the record's start-up transient dominates its peak-to-peak.
// A disc signal always carries the 7.6-9.3 MHz carriers as well.
TEST(RfCaptureTest, DoesNotClipAcrossTheCarrierRange) {
  for (const double speed : {kInnerCavSpeed, kOuterCavSpeed, kClvSpeed}) {
    for (const double carrier : {7.614286e6, 8.1e6, 9.314286e6, 12.15e6}) {
      if (carrier > 12e6 && speed != kOuterCavSpeed) {
        continue;
      }
      for (const bool equalise : {false, true}) {
        const Capture capture = CaptureCarrier(
            CaptureProfile::kDomesdayDuplicator, equalise, carrier, speed, 400);
        EXPECT_EQ(capture.clipped, 0) << speed << " " << carrier;
      }
    }
  }
}

TEST(RfCaptureTest, OutputDoesNotDependOnBlockSizeAndDecimatesByTwo) {
  const Capture reference = CaptureCarrier(CaptureProfile::kDomesdayDuplicator,
                                           false, 8.1e6, kClvSpeed, 100000);
  EXPECT_EQ(static_cast<std::int64_t>(reference.codes.size()),
            (reference.readout_samples + 1) / 2);
  for (const std::size_t block : {1U, 13U}) {
    EXPECT_EQ(CaptureCarrier(CaptureProfile::kDomesdayDuplicator, false, 8.1e6,
                             kClvSpeed, block)
                  .codes,
              reference.codes)
        << block;
  }
}

// The run length matches the carrier diagnostic's: n / rate < the run.
TEST(DiscRunLengthTest, CountsSamplesBeforeTheEndOfTheRun) {
  EXPECT_EQ(CaptureSampleCount(2), 2669334);
  EXPECT_EQ(CaptureSampleCount(3), 4004000);
  EXPECT_EQ(ReadoutSampleCount(3), 8008000);
  EXPECT_EQ(ReadoutSampleCount(1), 2669334);
  for (std::int64_t frames = 1; frames < 40; ++frames) {
    EXPECT_EQ(CaptureSampleCount(frames), (ReadoutSampleCount(frames) + 1) / 2);
  }
}

}  // namespace
}  // namespace ld_creator
