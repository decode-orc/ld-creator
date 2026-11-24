/*
 * File:        test_rf_sample_writer.cpp
 * Module:      rf_sample_writer
 * Purpose:     Unit tests for the S14 code-to-word mapping and .s16 layout
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

#include "ld_creator/rf_sample_writer/rf_sample_writer.h"
#include "memory_io.h"

namespace ld_creator {
namespace {

TEST(RfSampleWriterTest, MapsDomesdayDuplicatorCodesLikeACapture) {
  EXPECT_EQ(RfWordForCode(CaptureProfile::kDomesdayDuplicator, 0), -32768);
  EXPECT_EQ(RfWordForCode(CaptureProfile::kDomesdayDuplicator, 512), 0);
  EXPECT_EQ(RfWordForCode(CaptureProfile::kDomesdayDuplicator, 1023), 32704);
}

TEST(RfSampleWriterTest, PassesIdealCodesThrough) {
  EXPECT_EQ(RfWordForCode(CaptureProfile::kIdeal, -32768), -32768);
  EXPECT_EQ(RfWordForCode(CaptureProfile::kIdeal, 1234), 1234);
}

TEST(RfSampleWriterTest, RejectsCodesOutsideTheProfileRange) {
  EXPECT_THROW(RfWordForCode(CaptureProfile::kDomesdayDuplicator, 1024),
               std::out_of_range);
  EXPECT_THROW(RfWordForCode(CaptureProfile::kDomesdayDuplicator, -1),
               std::out_of_range);
  EXPECT_THROW(RfWordForCode(CaptureProfile::kIdeal, 40000), std::out_of_range);
}

TEST(RfSampleWriterTest, WritesLittleEndianWordsWithNoHeader) {
  test::MemorySinkFactory factory;
  const auto sink = factory.Create("/rf.s16");
  RfSampleWriter writer(CaptureProfile::kDomesdayDuplicator, sink.get());

  writer.Write({.first_sample = 0, .codes = {0, 513}});
  writer.Write({.first_sample = 2, .codes = {1023}});
  writer.Finish();

  const std::vector<std::byte>& bytes = factory.files.at("/rf.s16");
  ASSERT_EQ(bytes.size(), 6U);
  // −32768 = 0x8000; 64 = 0x0040; 32704 = 0x7FC0.
  EXPECT_EQ(bytes[0], std::byte{0x00});
  EXPECT_EQ(bytes[1], std::byte{0x80});
  EXPECT_EQ(bytes[2], std::byte{0x40});
  EXPECT_EQ(bytes[3], std::byte{0x00});
  EXPECT_EQ(bytes[4], std::byte{0xC0});
  EXPECT_EQ(bytes[5], std::byte{0x7F});
  EXPECT_EQ(writer.SamplesWritten(), 3);
}

TEST(RfSampleWriterTest, RejectsABlockThatLeavesAGap) {
  test::MemorySinkFactory factory;
  const auto sink = factory.Create("/rf.s16");
  RfSampleWriter writer(CaptureProfile::kIdeal, sink.get());

  EXPECT_THROW(writer.Write({.first_sample = 5, .codes = {1}}),
               std::logic_error);
}

}  // namespace
}  // namespace ld_creator
