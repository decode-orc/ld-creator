/*
 * File:        test_recording_signal_combiner.cpp
 * Module:      tests
 * Purpose:     Unit tests for S6, the recording signal combiner
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include "ld_creator/recording_signal_combiner/recording_signal_combiner.h"

namespace ld_creator {
namespace {

// Plan Task 4.1 acceptance: video only, D3 out equals D3 in.
TEST(RecordingSignalCombinerTest, PassesTheCarrierPhaseThroughUnchanged) {
  CarrierPhaseBlock input;
  input.first_sample = 1234;
  input.phase_rad = {0.0, 0.6, 1.2, 1.9};
  input.frequency_hz = {8.1e6, 8.2e6, 8.3e6, 8.4e6};
  input.slice_level = {0.0, 0.0, 0.0, 0.0};

  PassThroughSignalCombiner combiner;
  CarrierPhaseBlock output;
  combiner.Process(input, &output);
  EXPECT_EQ(output.first_sample, input.first_sample);
  EXPECT_EQ(output.phase_rad, input.phase_rad);
  EXPECT_EQ(output.frequency_hz, input.frequency_hz);
  EXPECT_EQ(output.slice_level, input.slice_level);

  combiner.Finish(&output);
  EXPECT_TRUE(output.phase_rad.empty());
  EXPECT_TRUE(output.frequency_hz.empty());
  EXPECT_TRUE(output.slice_level.empty());
}

}  // namespace
}  // namespace ld_creator
