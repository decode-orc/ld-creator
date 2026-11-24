/*
 * File:        test_carrier_diagnostic.cpp
 * Module:      tests
 * Purpose:     Unit tests for the capture quantiser and carrier diagnostic
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

#include "ld_creator/carrier_diagnostic/carrier_diagnostic_renderer.h"
#include "ld_creator/rf_capture/capture_quantiser.h"
#include "ld_creator/video_rate_converter/video_rate_converter.h"

namespace ld_creator {
namespace {

TEST(CaptureQuantiserTest, MapsTheDomesdayDuplicatorRange) {
  EXPECT_EQ(QuantiseCaptureInput(CaptureProfile::kDomesdayDuplicator, 0.0).code,
            512);
  EXPECT_EQ(
      QuantiseCaptureInput(CaptureProfile::kDomesdayDuplicator, -1.0).code, 0);
  const QuantisedCode top =
      QuantiseCaptureInput(CaptureProfile::kDomesdayDuplicator, 1.0);
  EXPECT_EQ(top.code, 1023);
  EXPECT_TRUE(top.clipped);
  EXPECT_FALSE(
      QuantiseCaptureInput(CaptureProfile::kDomesdayDuplicator, 0.75).clipped);
  // round(512 + 512·0.001) = round(512.512) = 513.
  EXPECT_EQ(
      QuantiseCaptureInput(CaptureProfile::kDomesdayDuplicator, 0.001).code,
      513);
}

TEST(CaptureQuantiserTest, MapsTheIdealSixteenBitRange) {
  EXPECT_EQ(QuantiseCaptureInput(CaptureProfile::kIdeal, 0.0).code, 0);
  EXPECT_EQ(QuantiseCaptureInput(CaptureProfile::kIdeal, -1.0).code, -32768);
  EXPECT_FALSE(QuantiseCaptureInput(CaptureProfile::kIdeal, -1.0).clipped);
  const QuantisedCode top = QuantiseCaptureInput(CaptureProfile::kIdeal, 1.0);
  EXPECT_EQ(top.code, 32767);
  EXPECT_TRUE(top.clipped);
  EXPECT_EQ(QuantiseCaptureInput(CaptureProfile::kIdeal, 0.5).code, 16384);
}

TEST(CarrierDiagnosticTest, CaptureSamplesAdvance189Over88ModellingSamples) {
  const CaptureToModellingRatio ratio = CaptureToModellingRate();
  EXPECT_EQ(ratio.numerator, 189);
  EXPECT_EQ(ratio.denominator, 88);
  EXPECT_NEAR(kModellingRateHz / static_cast<double>(kCaptureRateHz),
              189.0 / 88.0, 1e-15);
}

// A constant carrier of frequency f with φ = 2πf·t.
CarrierPhaseBlock ConstantCarrier(double frequency_hz, std::int64_t samples) {
  CarrierPhaseBlock block;
  for (std::int64_t m = 0; m < samples; ++m) {
    block.phase_rad.push_back(2.0 * std::numbers::pi * frequency_hz *
                              static_cast<double>(m) / kModellingRateHz);
    block.frequency_hz.push_back(frequency_hz);
    block.slice_level.push_back(0.0);
  }
  return block;
}

std::vector<std::int32_t> Render(const RfCaptureConfig& capture,
                                 const CarrierPhaseBlock& all,
                                 std::size_t block) {
  CarrierDiagnosticRenderer renderer(capture);
  std::vector<std::int32_t> codes;
  RfCodeBlock out;
  for (std::size_t start = 0; start < all.phase_rad.size(); start += block) {
    const std::size_t end = std::min(all.phase_rad.size(), start + block);
    CarrierPhaseBlock part;
    part.first_sample = static_cast<std::int64_t>(start);
    const auto from = static_cast<std::ptrdiff_t>(start);
    const auto to = static_cast<std::ptrdiff_t>(end);
    part.phase_rad.assign(all.phase_rad.begin() + from,
                          all.phase_rad.begin() + to);
    part.frequency_hz.assign(all.frequency_hz.begin() + from,
                             all.frequency_hz.begin() + to);
    part.slice_level.assign(all.slice_level.begin() + from,
                            all.slice_level.begin() + to);
    renderer.Process(part, &out);
    EXPECT_EQ(out.first_sample, static_cast<std::int64_t>(codes.size()));
    codes.insert(codes.end(), out.codes.begin(), out.codes.end());
  }
  renderer.Finish(&out);
  codes.insert(codes.end(), out.codes.begin(), out.codes.end());
  return codes;
}

TEST(CarrierDiagnosticTest, RendersOneFrameAsTheCaptureSamplesBeforeItsEnd) {
  // One frame: 2 866 500 D2 samples, so 2 866 501 D3 samples. Its length is
  // 1001/30000 s, and 40 MHz × 1001/30000 = 1 334 666.7, so 1 334 667
  // samples fall before the end.
  const CarrierPhaseBlock carrier =
      ConstantCarrier(8.1e6, kModellingSamplesPerFrame + 1);
  EXPECT_EQ(Render(RfCaptureConfig{}, carrier, 100000).size(), 1334667U);
}

TEST(CarrierDiagnosticTest, RendersTheQuantisedCosineOfTheCarrier) {
  constexpr double kFrequency = 8.1e6;
  const CarrierPhaseBlock carrier = ConstantCarrier(kFrequency, 20001);
  RfCaptureConfig capture;
  capture.profile = CaptureProfile::kIdeal;
  capture.level_fraction = 0.75;
  const std::vector<std::int32_t> codes = Render(capture, carrier, 20001);
  ASSERT_FALSE(codes.empty());
  for (std::size_t n = 0; n < codes.size(); ++n) {
    const double tau = static_cast<double>(n) / 40.0e6;
    const double expected =
        32768.0 * 0.75 * std::cos(2.0 * std::numbers::pi * kFrequency * tau);
    EXPECT_NEAR(codes[n], expected, 0.5 + 1e-6) << n;
  }
}

TEST(CarrierDiagnosticTest, OutputDoesNotDependOnBlockSize) {
  CarrierPhaseBlock carrier = ConstantCarrier(9.0e6, 5000);
  for (std::size_t m = 0; m < carrier.phase_rad.size(); ++m) {
    carrier.phase_rad[m] += 0.3 * std::sin(0.001 * static_cast<double>(m));
  }
  const RfCaptureConfig capture;
  const std::vector<std::int32_t> reference = Render(capture, carrier, 5000);
  for (const std::size_t block : {1U, 2U, 3U, 88U, 189U}) {
    EXPECT_EQ(Render(capture, carrier, block), reference) << block;
  }
}

}  // namespace
}  // namespace ld_creator
