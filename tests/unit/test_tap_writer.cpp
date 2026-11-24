/*
 * File:        test_tap_writer.cpp
 * Module:      diagnostics
 * Purpose:     Unit tests for the sampled and edge-list tap formats
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <bit>
#include <cstring>
#include <string>
#include <vector>

#include "ld_creator/diagnostics/tap_writer.h"
#include "memory_io.h"

namespace ld_creator {
namespace {

double ReadFloat64(const std::vector<std::byte>& bytes, std::size_t offset) {
  std::uint64_t bits = 0;
  for (std::size_t i = 0; i < 8; ++i) {
    bits |= static_cast<std::uint64_t>(bytes[offset + i]) << (8 * i);
  }
  return std::bit_cast<double>(bits);
}

const TapIdentity kIdentity{.stage = "cvbs_input",
                            .representation = "D1",
                            .unit = "IRE",
                            .first_frame = 2,
                            .frame_count = 1};

TEST(TapWriterTest, WritesOnlyTheSampleRangeWhateverTheBlockSplit) {
  test::MemorySinkFactory factory;
  SampledTapWriter writer(&factory, "/t/d1", kIdentity,
                          {.rate_hz = 4.0, .origin_samples = 0.5}, 3, 7);
  const std::vector<double> samples = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  writer.Write(0, std::span(samples).subspan(0, 4));
  writer.Write(4, std::span(samples).subspan(4, 1));
  writer.Write(5, std::span(samples).subspan(5, 5));
  writer.Finish();

  const std::vector<std::byte>& data = factory.files.at("/t/d1.f64");
  ASSERT_EQ(data.size(), 4U * 8U);
  for (std::size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(ReadFloat64(data, 8 * i), 3.0 + static_cast<double>(i));
  }
  const std::string descriptor = factory.Text("/t/d1.tap.yaml");
  EXPECT_NE(descriptor.find("format: ld-creator-tap\n"), std::string::npos);
  EXPECT_NE(descriptor.find("stage: cvbs_input\n"), std::string::npos);
  EXPECT_NE(descriptor.find("representation: D1\n"), std::string::npos);
  EXPECT_NE(descriptor.find("kind: sampled\n"), std::string::npos);
  EXPECT_NE(descriptor.find("data_file: \"d1.f64\"\n"), std::string::npos);
  EXPECT_NE(descriptor.find("sample_type: float64_le\n"), std::string::npos);
  EXPECT_NE(descriptor.find("rate_hz: 4\n"), std::string::npos);
  EXPECT_NE(descriptor.find("first_sample: 3\n"), std::string::npos);
  // (3 + 0.5) / 4 s.
  EXPECT_NE(descriptor.find("time_of_first_sample_s: 0.875\n"),
            std::string::npos);
  EXPECT_NE(descriptor.find("sample_count: 4\n"), std::string::npos);
  EXPECT_NE(descriptor.find("first_frame: 2\n"), std::string::npos);
}

TEST(TapWriterTest, WritesRecordedEdgesAsNineByteRecordsInRange) {
  test::MemorySinkFactory factory;
  EdgeTapWriter writer(&factory, "/t/d4", kIdentity,
                       EdgeRecordLayout::kRecordedEdge, 1.0, 3.0);
  const std::vector<RecordedEdge> edges = {
      {.time_s = 0.5, .polarity = EdgePolarity::kPitStart},
      {.time_s = 1.0, .polarity = EdgePolarity::kPitEnd},
      {.time_s = 2.5, .polarity = EdgePolarity::kPitStart},
      {.time_s = 3.0, .polarity = EdgePolarity::kPitEnd}};

  writer.Write(std::span(edges));
  writer.Finish();

  const std::vector<std::byte>& data = factory.files.at("/t/d4.edges");
  ASSERT_EQ(data.size(), 18U);
  EXPECT_EQ(ReadFloat64(data, 0), 1.0);
  EXPECT_EQ(static_cast<std::int8_t>(data[8]), -1);
  EXPECT_EQ(ReadFloat64(data, 9), 2.5);
  EXPECT_EQ(static_cast<std::int8_t>(data[17]), 1);
  const std::string descriptor = factory.Text("/t/d4.tap.yaml");
  EXPECT_NE(descriptor.find("kind: edges\n"), std::string::npos);
  EXPECT_NE(descriptor.find("record_bytes: 9\n"), std::string::npos);
  EXPECT_NE(descriptor.find("record_count: 2\n"), std::string::npos);
}

TEST(TapWriterTest, WritesPitEdgesAsSeventeenByteRecords) {
  test::MemorySinkFactory factory;
  EdgeTapWriter writer(&factory, "/t/d5", kIdentity, EdgeRecordLayout::kPitEdge,
                       0.0, 1.0);
  const std::vector<PitEdge> edges = {{.arc_length_m = 0.25,
                                       .radius_m = 0.055,
                                       .polarity = EdgePolarity::kPitEnd}};

  writer.Write(std::span(edges));
  writer.Finish();

  const std::vector<std::byte>& data = factory.files.at("/t/d5.edges");
  ASSERT_EQ(data.size(), 17U);
  EXPECT_EQ(ReadFloat64(data, 0), 0.25);
  EXPECT_EQ(ReadFloat64(data, 8), 0.055);
  EXPECT_EQ(static_cast<std::int8_t>(data[16]), -1);
  EXPECT_NE(factory.Text("/t/d5.tap.yaml").find("record_bytes: 17\n"),
            std::string::npos);
}

}  // namespace
}  // namespace ld_creator
