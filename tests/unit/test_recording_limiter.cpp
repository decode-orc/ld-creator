/*
 * File:        test_recording_limiter.cpp
 * Module:      tests
 * Purpose:     Unit tests for S7, the recording limiter
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
#include <stdexcept>
#include <vector>

#include "ld_creator/fm_phase_modulator/fm_phase_modulator.h"
#include "ld_creator/recording_limiter/recording_limiter.h"
#include "ld_creator/video_rate_converter/video_rate_converter.h"

namespace ld_creator {
namespace {

// S7 note: the edge-time budget for S2 + S5 + S7 (register S7.edge_budget).
constexpr double kEdgeBudgetS = 4e-12;

constexpr double kTwoPiValue = 2.0 * std::numbers::pi;

// D3 for a phase law φ(t) and frequency f(t), sampled at Fm.
template <typename Phase, typename Frequency>
CarrierPhaseBlock SampleCarrier(std::int64_t samples, Phase phase,
                                Frequency frequency) {
  CarrierPhaseBlock block;
  for (std::int64_t m = 0; m < samples; ++m) {
    const double t = static_cast<double>(m) / kModellingRateHz;
    block.phase_rad.push_back(phase(t));
    block.frequency_hz.push_back(frequency(t));
    block.slice_level.push_back(0.0);
  }
  return block;
}

// Runs the limiter over `input` split into blocks of `block` samples and
// returns every edge, checking the completion watermark on the way.
std::vector<RecordedEdge> RunLimiter(const CarrierPhaseBlock& input,
                                     std::size_t block) {
  RecordingLimiter limiter;
  std::vector<RecordedEdge> edges;
  RecordedEdgeBlock out;
  for (std::size_t start = 0; start < input.phase_rad.size(); start += block) {
    const std::size_t end = std::min(input.phase_rad.size(), start + block);
    const auto from = static_cast<std::ptrdiff_t>(start);
    const auto to = static_cast<std::ptrdiff_t>(end);
    CarrierPhaseBlock part;
    part.first_sample = static_cast<std::int64_t>(start);
    part.phase_rad.assign(input.phase_rad.begin() + from,
                          input.phase_rad.begin() + to);
    part.frequency_hz.assign(input.frequency_hz.begin() + from,
                             input.frequency_hz.begin() + to);
    part.slice_level.assign(input.slice_level.begin() + from,
                            input.slice_level.begin() + to);
    limiter.Process(part, &out);
    for (const RecordedEdge& edge : out.edges) {
      EXPECT_LE(edge.time_s, out.complete_until_s);
    }
    EXPECT_DOUBLE_EQ(out.complete_until_s,
                     static_cast<double>(end - 1) / kModellingRateHz);
    edges.insert(edges.end(), out.edges.begin(), out.edges.end());
  }
  limiter.Finish(&out);
  EXPECT_TRUE(out.edges.empty());
  return edges;
}

void ExpectAlternatingPolarity(const std::vector<RecordedEdge>& edges) {
  for (std::size_t i = 1; i < edges.size(); ++i) {
    ASSERT_NE(edges[i].polarity, edges[i - 1].polarity) << "edge " << i;
  }
}

class ConstantCarrierTest : public ::testing::TestWithParam<double> {};

// Plan Task 4.2 acceptance: on a constant-frequency input, edge intervals
// equal 1/(2f) to the documented bound, and edges alternate polarity. The
// frequencies are sync tip, blanking, white and the largest overshoot.
TEST_P(ConstantCarrierTest, EdgeIntervalsAreHalfACarrierPeriod) {
  const double frequency = GetParam();
  constexpr double kInitialPhase = 0.3;
  const CarrierPhaseBlock carrier = SampleCarrier(
      100000,
      [frequency](double t) {
        return kInitialPhase + (kTwoPiValue * frequency * t);
      },
      [frequency](double /*t*/) { return frequency; });
  const std::vector<RecordedEdge> edges = RunLimiter(carrier, 4096);

  const double duration = 99999.0 / kModellingRateHz;
  ASSERT_EQ(edges.size(),
            static_cast<std::size_t>(std::floor(
                (kInitialPhase + (kTwoPiValue * frequency * duration)) /
                    std::numbers::pi -
                0.5)) +
                1);
  ExpectAlternatingPolarity(edges);
  for (std::size_t i = 1; i < edges.size(); ++i) {
    ASSERT_NEAR(edges[i].time_s - edges[i - 1].time_s, 0.5 / frequency,
                kEdgeBudgetS)
        << "edge " << i;
  }
}

INSTANTIATE_TEST_SUITE_P(CarrierFrequencies, ConstantCarrierTest,
                         ::testing::Values(7.614286e6, 8.1e6, 9.314286e6,
                                           12.15e6));

// The same check through the real S5 output for constant −40, 0 and 100 IRE:
// the carrier S7 sees is the one S5 integrates.
TEST(RecordingLimiterTest, CutsTheFmModulatorCarrierAtHalfPeriods) {
  for (const double ire : {-40.0, 0.0, 100.0}) {
    FmPhaseModulator modulator(FmPhaseModulatorConfig{});
    IreSampleBlock video;
    video.ire.assign(50000, ire);
    CarrierPhaseBlock carrier;
    modulator.Process(video, &carrier);
    const std::vector<RecordedEdge> edges = RunLimiter(carrier, 1000);
    const double frequency = 8.1e6 + (ire * 1.7e6 / 140.0);
    ExpectAlternatingPolarity(edges);
    // Away from the run's start, where the integration rule sees the
    // 0 IRE outside the run.
    for (std::size_t i = 10; i < edges.size(); ++i) {
      ASSERT_NEAR(edges[i].time_s - edges[i - 1].time_s, 0.5 / frequency,
                  kEdgeBudgetS)
          << ire << " IRE, edge " << i;
    }
  }
}

// A linear frequency sweep has quadratic phase, which the cubic Hermite
// interpolant reproduces exactly, so every edge must land on the analytic
// root of 2π(f₀t + at²/2) = (k + ½)π. This exercises the solver where the
// interpolant is not a straight line.
TEST(RecordingLimiterTest, EdgesOfASweptCarrierMatchTheAnalyticRoots) {
  constexpr double kStartHz = 8.1e6;
  // 8.1 → 9.3 MHz over 1 ms.
  constexpr double kSweepHzPerS = 1.2e9;
  const CarrierPhaseBlock carrier = SampleCarrier(
      85910,
      [](double t) {
        return kTwoPiValue * ((kStartHz * t) + (0.5 * kSweepHzPerS * t * t));
      },
      [](double t) { return kStartHz + (kSweepHzPerS * t); });
  const std::vector<RecordedEdge> edges = RunLimiter(carrier, 777);
  ASSERT_GT(edges.size(), 17000U);
  for (std::size_t k = 0; k < edges.size(); ++k) {
    // a·t²/2 + f₀·t − c = 0 with c = (k + ½)/2, in the stable form.
    const double c = (static_cast<double>(k) + 0.5) / 2.0;
    const double expected = 2.0 * c /
                            (kStartHz + std::sqrt((kStartHz * kStartHz) +
                                                  (2.0 * kSweepHzPerS * c)));
    ASSERT_NEAR(edges[k].time_s, expected, kEdgeBudgetS) << "edge " << k;
  }
}

// S7 note: the pit is where cos φ > 0. Starting at φ = 0 the track is in a
// pit, so the first edge, at φ = π/2, ends it.
TEST(RecordingLimiterTest, FirstEdgeFromZeroPhaseEndsAPit) {
  const CarrierPhaseBlock carrier = SampleCarrier(
      100, [](double t) { return kTwoPiValue * 8.1e6 * t; },
      [](double /*t*/) { return 8.1e6; });
  const std::vector<RecordedEdge> edges = RunLimiter(carrier, 100);
  ASSERT_FALSE(edges.empty());
  EXPECT_EQ(edges.front().polarity, EdgePolarity::kPitEnd);
  EXPECT_NEAR(edges.front().time_s, 0.25 / 8.1e6, 1e-15);
  ASSERT_GE(edges.size(), 2U);
  EXPECT_EQ(edges[1].polarity, EdgePolarity::kPitStart);
}

TEST(RecordingLimiterTest, OutputDoesNotDependOnBlockSize) {
  const CarrierPhaseBlock carrier = SampleCarrier(
      20000,
      [](double t) {
        return 1.0 + (kTwoPiValue * 8.1e6 * t) +
               (0.4 * std::sin(kTwoPiValue * 1.0e6 * t));
      },
      [](double t) {
        return 8.1e6 + (0.4 * 1.0e6 * std::cos(kTwoPiValue * 1.0e6 * t));
      });
  const std::vector<RecordedEdge> whole = RunLimiter(carrier, 20000);
  for (const std::size_t block : {1U, 2U, 7U, 4096U}) {
    const std::vector<RecordedEdge> split = RunLimiter(carrier, block);
    ASSERT_EQ(split.size(), whole.size()) << block;
    for (std::size_t i = 0; i < whole.size(); ++i) {
      ASSERT_EQ(split[i].time_s, whole[i].time_s) << block << ", " << i;
      ASSERT_EQ(split[i].polarity, whole[i].polarity) << block << ", " << i;
    }
  }
}

TEST(RecordingLimiterTest, RejectsANonZeroSliceLevel) {
  CarrierPhaseBlock carrier = SampleCarrier(
      10, [](double t) { return kTwoPiValue * 8.1e6 * t; },
      [](double /*t*/) { return 8.1e6; });
  carrier.slice_level[5] = 0.01;
  RecordingLimiter limiter;
  RecordedEdgeBlock out;
  EXPECT_THROW(limiter.Process(carrier, &out), std::invalid_argument);
}

TEST(RecordingLimiterTest, RejectsAPhaseThatDoesNotIncrease) {
  CarrierPhaseBlock carrier;
  carrier.phase_rad = {0.0, 0.5, 0.4};
  carrier.frequency_hz = {8.1e6, 8.1e6, 8.1e6};
  carrier.slice_level = {0.0, 0.0, 0.0};
  RecordingLimiter limiter;
  RecordedEdgeBlock out;
  EXPECT_THROW(limiter.Process(carrier, &out), std::runtime_error);
}

}  // namespace
}  // namespace ld_creator
