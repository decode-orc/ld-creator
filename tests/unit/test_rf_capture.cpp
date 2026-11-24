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
#include <numbers>
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

// The documented disagreement (S13 note, "Open items"): from 6.8 to
// 12.1 MHz the measured response is up to 2.9 dB below the nominal model,
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

TEST(DddCaptureFilterTest, FirRealisesTheContinuousFilterInTheReadoutBand) {
  const FirFilter fir = DesignDddAntiAliasFir();
  for (double f = 0.0; f <= 35.7e6; f += 0.1e6) {
    const ComplexValue realised = fir.ResponseAt(f, kReadoutRateHz);
    const ComplexValue exact = DddAntiAliasResponse(f);
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
// documented amount (S13 note): DdD at least 47.3 dB below its 1 MHz level
// (nominal components), ideal at least 98 dB.
TEST(CaptureChainTest, AttenuatesAboveTwentyMegahertzAsDocumented) {
  const CaptureChain ddd =
      MakeCaptureChain(CaptureProfile::kDomesdayDuplicator);
  const double reference = Magnitude(ddd.ResponseAt(1e6));
  double ddd_worst = -1e9;
  for (double f = 20e6; f <= 40e6; f += 0.05e6) {
    ddd_worst =
        std::max(ddd_worst, Db(Magnitude(ddd.ResponseAt(f)) / reference));
  }
  EXPECT_LT(ddd_worst, -47.3);
  EXPECT_GT(ddd_worst, -47.4);

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
};
Capture CaptureCarrier(CaptureProfile profile, bool equalise,
                       double frequency_hz, double blanking_hz, double speed,
                       std::size_t edges_per_block) {
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
  const CaptureChain chain = MakeCaptureChain(profile);
  const CaptureLevel level = ComputeCaptureLevel(
      {.profile = profile, .level_fraction = 0.75}, chain,
      [transfer](double nu) { return transfer->Transfer(nu); },
      readout.Contrast(), blanking_hz, speed);
  RfCapture capture(profile, chain, level);
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
  return result;
}

constexpr double kInnerCavSpeed = 10.3577;
constexpr double kOuterCavSpeed = 27.3066;
constexpr double kClvSpeed = 11.0;

// The level rule (S13 note, "Level"): the blanking carrier has a
// peak-to-peak of level_fraction of full scale. An 8 MHz carrier is 5
// capture samples per period, so a fit over whole periods is exact; at
// 55 mm its harmonics are beyond the optical cut-off.
TEST(RfCaptureTest, BlankingCarrierFillsTheLevelFractionOfFullScale) {
  for (const CaptureProfile profile :
       {CaptureProfile::kDomesdayDuplicator, CaptureProfile::kIdeal}) {
    for (const bool equalise : {false, true}) {
      const Capture capture =
          CaptureCarrier(profile, equalise, 8e6, 8e6, kInnerCavSpeed, 400);
      // Skip 25 µs at each end (filter start-up and the record's edges).
      const std::size_t skip = 1000;
      const std::size_t count = ((capture.codes.size() - (2 * skip)) / 5) * 5;
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
      const double amplitude = test::FitSinusoid(values, times, 8e6).amplitude;
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
// 12.15 MHz) at both CAV radii and on CLV.
TEST(RfCaptureTest, DoesNotClipAcrossTheCarrierRange) {
  for (const double speed : {kInnerCavSpeed, kOuterCavSpeed, kClvSpeed}) {
    for (const double carrier : {7.614286e6, 8.1e6, 9.314286e6, 12.15e6}) {
      for (const bool equalise : {false, true}) {
        const Capture capture =
            CaptureCarrier(CaptureProfile::kDomesdayDuplicator, equalise,
                           carrier, 8.1e6, speed, 400);
        EXPECT_EQ(capture.clipped, 0) << speed << " " << carrier;
      }
    }
  }
}

TEST(RfCaptureTest, OutputDoesNotDependOnBlockSizeAndDecimatesByTwo) {
  const Capture reference =
      CaptureCarrier(CaptureProfile::kDomesdayDuplicator, false, 8.1e6, 8.1e6,
                     kClvSpeed, 100000);
  EXPECT_EQ(static_cast<std::int64_t>(reference.codes.size()),
            (reference.readout_samples + 1) / 2);
  for (const std::size_t block : {1U, 13U}) {
    EXPECT_EQ(CaptureCarrier(CaptureProfile::kDomesdayDuplicator, false, 8.1e6,
                             8.1e6, kClvSpeed, block)
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
