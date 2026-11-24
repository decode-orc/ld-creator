/*
 * File:        test_readout_transfer.cpp
 * Module:      optical_readout
 * Purpose:     Unit tests for the linear readout transfer function
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

#include "ld_creator/optical_readout/readout_transfer.h"

namespace ld_creator {
namespace {

constexpr double kNa = 0.40;
constexpr double kRim = 0.23;
constexpr double kCutoffPerM = 2.0 * kNa / kReadoutWavelengthM;

// Uniform circular pupil (rim intensity 1): the closed-form overlap of two
// unit discs, H(ν) = (2/π)·(acos(ν/2) − (ν/2)·√(1 − ν²/4)).
double UniformPupil(double nu) {
  const double half = nu / 2.0;
  return 2.0 / std::numbers::pi *
         (std::acos(half) - half * std::sqrt(1.0 - half * half));
}

TEST(ReadoutTransferTest, IsOneAtZeroFrequency) {
  EXPECT_NEAR(ReadoutTransfer(0.0, kNa, kRim, kReadoutWavelengthM), 1.0, 1e-12);
  EXPECT_NEAR(ReadoutTransfer(0.0, kNa, 1.0, kReadoutWavelengthM), 1.0, 1e-12);
}

TEST(ReadoutTransferTest, IsZeroAtAndAboveTheCutoff) {
  EXPECT_EQ(ReadoutTransfer(kCutoffPerM, kNa, kRim, kReadoutWavelengthM), 0.0);
  EXPECT_EQ(ReadoutTransfer(2.0 * kCutoffPerM, kNa, kRim, kReadoutWavelengthM),
            0.0);
}

TEST(ReadoutTransferTest, MatchesUniformPupilClosedForm) {
  for (const double fraction : {0.1, 0.3, 0.5, 0.634, 0.8, 0.95}) {
    const double nu = 2.0 * fraction;
    EXPECT_NEAR(
        ReadoutTransfer(fraction * kCutoffPerM, kNa, 1.0, kReadoutWavelengthM),
        UniformPupil(nu), 1e-12)
        << fraction;
  }
}

TEST(ReadoutTransferTest, GivesS11ValueAtTheHfmiReferenceFrequency) {
  // S11 note: H(802 pits/mm) = 0.2134 ± 0.002 for the 23 % rim pupil, and
  // 0.2507 for a uniform pupil.
  EXPECT_NEAR(ReadoutTransfer(kHfmiReferenceFrequencyPerM, kNa, kRim,
                              kReadoutWavelengthM),
              0.2134, 0.002);
  EXPECT_NEAR(ReadoutTransfer(kHfmiReferenceFrequencyPerM, kNa, 1.0,
                              kReadoutWavelengthM),
              0.2507, 0.0005);
}

TEST(ReadoutTransferTest, FallsMonotonicallyToTheCutoff) {
  double previous = 1.0;
  for (int step = 1; step <= 100; ++step) {
    const double value = ReadoutTransfer(step * 0.01 * kCutoffPerM, kNa, kRim,
                                         kReadoutWavelengthM);
    EXPECT_LT(value, previous) << step;
    previous = value;
  }
}

TEST(ReadoutTransferTest, MaximumHfmiIsFourOverPiTimesTransfer) {
  EXPECT_DOUBLE_EQ(MaximumHfmi(kNa, kRim),
                   4.0 / std::numbers::pi *
                       ReadoutTransfer(kHfmiReferenceFrequencyPerM, kNa, kRim,
                                       kReadoutWavelengthM));
}

}  // namespace
}  // namespace ld_creator
