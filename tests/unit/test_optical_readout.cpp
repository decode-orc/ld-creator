/*
 * File:        test_optical_readout.cpp
 * Module:      tests
 * Purpose:     Unit tests for S11, the linear optical readout
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "ld_creator/dsp/gauss_legendre.h"
#include "ld_creator/optical_readout/optical_readout.h"
#include "ld_creator/optical_readout/readout_kernel.h"
#include "ld_creator/optical_readout/readout_transfer.h"
#include "pit_train.h"
#include "sinusoid_fit.h"

namespace ld_creator {
namespace {

constexpr double kNa = 0.40;
constexpr double kRim = 0.23;
// CAV at 55 mm: 2π · 55 mm · 30000/1001 Hz.
constexpr double kInnerSpeed = 10.3577;

double Cutoff() { return 2.0 * kNa / kReadoutWavelengthM; }

const EdgeResponseTable& Table() {
  static const EdgeResponseTable table = MakeReadoutEdgeResponse(kNa, kRim);
  return table;
}

// The bound of the S11 note ("Rendering"): the readout kernel's transfer
// equals H within 1.1e-4 at every spatial frequency of 110 cycles/mm or more.
constexpr double kKernelTransferBound = 1.1e-4;
constexpr double kLowestPatternFrequency = 110e3;

TEST(ReadoutKernelTest,
     TransferMatchesTheOpticsAboveTheLowestPatternFrequency) {
  for (double nu = kLowestPatternFrequency; nu < 1.1 * Cutoff(); nu += 7e3) {
    const double h = ReadoutTransfer(nu, kNa, kRim, kReadoutWavelengthM);
    EXPECT_NEAR(Table().Transfer(nu), h, kKernelTransferBound) << nu;
  }
}

TEST(ReadoutKernelTest, LineSpreadFunctionIsTheTransformOfTheTransfer) {
  // ∫ LSF = H(0) = 1; the kernel keeps all but the tail beyond the window.
  EXPECT_NEAR(Table().Mass(), 0.99695, 1e-5);
  // LSF is non-negative (S11 note), so the edge response rises.
  const ReadoutLineSpread lsf(kNa, kRim);
  for (double u = 0.0; u < 20e-6; u += 0.013e-6) {
    EXPECT_GE(lsf(u), 0.0) << u;
  }
  EXPECT_NEAR(Table()(0.0), 0.5 * Table().Mass(), 1e-14);
}

TEST(ReadoutKernelTest, TableInterpolationErrorIsBelowTheStatedBound) {
  // E(u) − M/2 = ∫₀^u k, integrated directly with 64-point panels of 2 nm.
  const ReadoutLineSpread lsf(kNa, kRim);
  const GaussLegendreRule rule = MakeGaussLegendreRule(8);
  for (double target : {0.0123e-6, 0.3371e-6, 1.0007e-6, 4.4441e-6, 9.9e-6}) {
    double integral = 0.0;
    const int panels = static_cast<int>(std::ceil(target / 2e-9));
    const double width = target / panels;
    for (int p = 0; p < panels; ++p) {
      for (std::size_t q = 0; q < rule.nodes.size(); ++q) {
        integral += 0.5 * width * rule.weights[q] *
                    lsf(width * (p + (0.5 * (rule.nodes[q] + 1.0))));
      }
    }
    EXPECT_NEAR(Table()(target) - (0.5 * Table().Mass()), integral, 1e-10)
        << target;
  }
}

TEST(ReadoutKernelTest, PitContrastOfTheDefaultHfmiIsOne) {
  EXPECT_DOUBLE_EQ(PitContrast(MaximumHfmi(kNa, kRim), kNa, kRim), 1.0);
}

std::vector<double> Render(double period_m, double length_m, double hfmi,
                           std::size_t edges_per_block) {
  const test::ConstantSpeedTrajectory spot(kInnerSpeed);
  const auto samples = static_cast<std::int64_t>(
      std::floor(length_m / kInnerSpeed * kReadoutRateHz));
  LinearOpticalReadout readout(
      {.numerical_aperture = kNa, .rim_intensity = kRim, .hfmi = hfmi}, &spot,
      {.sample_count = samples, .end_arc_length_m = length_m});
  std::vector<double> levels;
  ReadoutBlock out;
  for (const ScanBlock& block :
       test::PitTrain(period_m, length_m, kInnerSpeed, edges_per_block)) {
    readout.Process(block, &out);
    EXPECT_EQ(out.photocurrent.first_sample,
              static_cast<std::int64_t>(levels.size()));
    levels.insert(levels.end(), out.photocurrent.level.begin(),
                  out.photocurrent.level.end());
  }
  readout.Finish(&out);
  levels.insert(levels.end(), out.photocurrent.level.begin(),
                out.photocurrent.level.end());
  EXPECT_EQ(static_cast<std::int64_t>(levels.size()), samples);
  return levels;
}

// Fits the fundamental and the mean over whole periods away from the ends.
// The mean is removed first: the fit has no constant term.
struct Fundamental {
  double amplitude;
  double mean;
};
Fundamental FitFundamental(const std::vector<double>& levels, double period_m) {
  const double frequency = kInnerSpeed / period_m;
  const double samples_per_period = kReadoutRateHz / frequency;
  const auto skip =
      static_cast<std::size_t>(100e-6 / kInnerSpeed * kReadoutRateHz);
  const double periods = std::floor(
      static_cast<double>(levels.size() - (2 * skip)) / samples_per_period);
  const auto count =
      static_cast<std::size_t>(std::round(periods * samples_per_period));
  double sum = 0.0;
  for (std::size_t n = skip; n < skip + count; ++n) {
    sum += levels[n];
  }
  const double mean = sum / static_cast<double>(count);
  std::vector<double> values;
  std::vector<double> times;
  for (std::size_t n = skip; n < skip + count; ++n) {
    values.push_back(levels[n] - mean);
    times.push_back(static_cast<double>(n) / kReadoutRateHz);
  }
  return {.amplitude = test::FitSinusoid(values, times, frequency).amplitude,
          .mean = mean};
}

// Plan Task 5.2 acceptance: a constant-frequency pit train gives the
// predicted modulation depth, and at 802 pits/mm that depth is the HFMI.
TEST(LinearOpticalReadoutTest, PitTrainGivesThePredictedModulationDepth) {
  const double hfmi = MaximumHfmi(kNa, kRim);
  for (double pits_per_mm : {500.0, 802.0, 1000.0}) {
    const double period = 1e-3 / pits_per_mm;
    const std::vector<double> levels = Render(period, 1.0e-3, hfmi, 1000);
    const Fundamental fit = FitFundamental(levels, period);
    const double h =
        ReadoutTransfer(1.0 / period, kNa, kRim, kReadoutWavelengthM);
    // m = 1: fundamental of d (2/π) times H.
    EXPECT_NEAR(fit.amplitude, 2.0 / std::numbers::pi * h, kKernelTransferBound)
        << pits_per_mm;
    EXPECT_NEAR(fit.mean, 0.5, 1e-4) << pits_per_mm;
    if (pits_per_mm == 802.0) {
      // HFMI = (V₂ − V₃)/V₁ with V₁ = 1 (S11 note).
      EXPECT_NEAR(2.0 * fit.amplitude, hfmi, 2e-4);
      EXPECT_GE(hfmi, 0.16 + (0.27 / 0.6) * 0.02);
      EXPECT_LE(hfmi, 0.40);
    }
  }
}

TEST(LinearOpticalReadoutTest, LowerHfmiScalesTheModulation) {
  const double period = 1e-3 / 802.0;
  const std::vector<double> full =
      Render(period, 0.5e-3, MaximumHfmi(kNa, kRim), 1000);
  const std::vector<double> half =
      Render(period, 0.5e-3, 0.5 * MaximumHfmi(kNa, kRim), 1000);
  for (std::size_t n = 0; n < full.size(); n += 97) {
    EXPECT_NEAR(1.0 - half[n], 0.5 * (1.0 - full[n]), 1e-14);
  }
}

TEST(LinearOpticalReadoutTest, OutputDoesNotDependOnBlockSize) {
  const double period = 1e-3 / 700.0;
  const double hfmi = MaximumHfmi(kNa, kRim);
  const std::vector<double> reference = Render(period, 0.2e-3, hfmi, 100000);
  for (std::size_t block : {1U, 7U, 64U}) {
    EXPECT_EQ(Render(period, 0.2e-3, hfmi, block), reference) << block;
  }
}

TEST(LinearOpticalReadoutTest, StaysWithinTheLinearModelsRange) {
  const std::vector<double> levels =
      Render(1e-3 / 300.0, 0.2e-3, MaximumHfmi(kNa, kRim), 50);
  for (const double level : levels) {
    EXPECT_GE(level, -1e-3);
    EXPECT_LE(level, 1.0 + 1e-3);
  }
}

// Plan Task 5.2: bound on treating pits as stripes across the track. For a
// pit of width w the linear transfer is H_w(ν) = ∫ H(√(ν² + η²))·w·sinc(wη)
// dη (the 2-D transfer is radially symmetric). With m set from the HFMI at
// 802 pits/mm, the shape error at ν is H_w(ν)·H(ν₀)/(H_w(ν₀)·H(ν)) − 1.
TEST(LinearOpticalReadoutTest, FiniteWidthPitChangesTheShapeByLessThanBound) {
  const double width = 0.8e-6;
  const double fc = Cutoff();
  const GaussLegendreRule rule = MakeGaussLegendreRule(64);
  const auto finite = [&](double nu) {
    const double reach = std::sqrt((fc * fc) - (nu * nu));
    double sum = 0.0;
    constexpr int kPanels = 16;
    for (int p = 0; p < kPanels; ++p) {
      for (std::size_t q = 0; q < rule.nodes.size(); ++q) {
        const double eta = -reach + (2.0 * reach / kPanels) *
                                        (p + (0.5 * (rule.nodes[q] + 1.0)));
        const double x = std::numbers::pi * width * eta;
        const double sinc = x == 0.0 ? 1.0 : std::sin(x) / x;
        sum += (reach / kPanels) * rule.weights[q] * width * sinc *
               ReadoutTransfer(std::hypot(nu, eta), kNa, kRim,
                               kReadoutWavelengthM);
      }
    }
    return sum;
  };
  const double reference = kHfmiReferenceFrequencyPerM;
  const double href =
      ReadoutTransfer(reference, kNa, kRim, kReadoutWavelengthM);
  const double wref = finite(reference);
  double worst = 0.0;
  for (double nu = 300e3; nu <= 0.833 * fc; nu += 20e3) {
    const double h = ReadoutTransfer(nu, kNa, kRim, kReadoutWavelengthM);
    worst = std::max(worst, std::abs((finite(nu) * href / (wref * h)) - 1.0));
  }
  // Recorded in the S11 note.
  EXPECT_LT(worst, 0.19);
  EXPECT_GT(worst, 0.17);
}

}  // namespace
}  // namespace ld_creator
