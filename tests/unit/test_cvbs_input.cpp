/*
 * File:        test_cvbs_input.cpp
 * Module:      cvbs_input
 * Purpose:     Unit tests for S1: encodings to IRE, frame counting, blocks
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "ld_creator/cvbs_input/cvbs_metadata.h"
#include "ld_creator/cvbs_input/cvbs_video_input.h"
#include "ld_creator/standards/ntsc_timing.h"
#include "memory_io.h"

namespace ld_creator {
namespace {

constexpr double kTolerance = 1e-9;

// Decodes stored words with `encoding` to IRE.
std::vector<double> Decode(SampleEncoding encoding,
                           const std::vector<std::uint16_t>& words) {
  const std::vector<std::byte> bytes = test::WordsToBytes(words);
  std::vector<double> ire(words.size());
  DecodeSampleWords(encoding, bytes, ire);
  return ire;
}

// Stored word for 10-bit value v in each encoding (CVBS spec).
std::uint16_t Word(SampleEncoding encoding, int v) {
  switch (encoding) {
    case SampleEncoding::kU10:
      return static_cast<std::uint16_t>(static_cast<std::int16_t>(v));
    case SampleEncoding::kU16:
      return static_cast<std::uint16_t>(v * 64);
    case SampleEncoding::kS16:
      return static_cast<std::uint16_t>(
          static_cast<std::int16_t>((v - 240) * 32));
    case SampleEncoding::kTpg21:
      return static_cast<std::uint16_t>(
          static_cast<std::int16_t>((v - 508) * 64));
  }
  return 0;
}

TEST(CvbsInputTest, MapsSyncBlankingBlackAndWhiteToIreInEveryEncoding) {
  for (const SampleEncoding encoding :
       {SampleEncoding::kU10, SampleEncoding::kU16, SampleEncoding::kS16,
        SampleEncoding::kTpg21}) {
    const std::vector<double> ire =
        Decode(encoding, {Word(encoding, 16), Word(encoding, 240),
                          Word(encoding, 282), Word(encoding, 800)});
    EXPECT_NEAR(ire[0], -40.0, kTolerance);
    EXPECT_NEAR(ire[1], 0.0, kTolerance);
    EXPECT_NEAR(ire[2], 7.5, kTolerance);
    EXPECT_NEAR(ire[3], 100.0, kTolerance);
  }
}

TEST(CvbsInputTest, KeepsValuesBelowSyncTipWithTheirSign) {
  // CVBS_S16_4FSC carries the LaserVision VBI burst below code 0: here
  // 10-bit value −100, about −60.7 IRE.
  const std::vector<double> s16 = Decode(
      SampleEncoding::kS16,
      {static_cast<std::uint16_t>(static_cast<std::int16_t>(-340 * 32))});
  EXPECT_NEAR(s16[0], -340.0 / 5.6, kTolerance);

  const std::vector<double> u10 =
      Decode(SampleEncoding::kU10,
             {static_cast<std::uint16_t>(static_cast<std::int16_t>(-20))});
  EXPECT_NEAR(u10[0], (-20.0 - 240.0) / 5.6, kTolerance);
}

TEST(CvbsInputTest, AcceptsNtscCompositeStableInputInEachEncoding) {
  CvbsMetadata metadata{.preset = "NTSC",
                        .sample_encoding_preset = "CVBS_TPG21_4FSC",
                        .signal_state_preset = "STANDARD_STABLE_UNLOCKED",
                        .signal_type = "composite"};
  const InputFormatResult result = CheckNtscCompositeInput(metadata);

  if (!result.encoding.has_value()) {
    FAIL() << result.error;
  }
  EXPECT_EQ(result.encoding.value(), SampleEncoding::kTpg21);
}

TEST(CvbsInputTest, RejectsInputThatIsNotNtscComposite4fscStable) {
  const CvbsMetadata good{.preset = "NTSC",
                          .sample_encoding_preset = "CVBS_U10_4FSC",
                          .signal_state_preset = "STANDARD_STABLE_LOCKED",
                          .signal_type = "composite"};
  if (!CheckNtscCompositeInput(good).encoding.has_value()) {
    FAIL();
  }

  CvbsMetadata pal = good;
  pal.preset = "PAL";
  EXPECT_NE(CheckNtscCompositeInput(pal).error.find("only NTSC"),
            std::string::npos);

  CvbsMetadata yc = good;
  yc.signal_type = "yc";
  EXPECT_NE(CheckNtscCompositeInput(yc).error.find("composite"),
            std::string::npos);

  CvbsMetadata raw = good;
  raw.signal_state_preset = "STANDARD_RAW";
  EXPECT_NE(CheckNtscCompositeInput(raw).error.find("STANDARD_RAW"),
            std::string::npos);

  CvbsMetadata encoding = good;
  encoding.sample_encoding_preset = "RAW_S16_40M";
  EXPECT_NE(CheckNtscCompositeInput(encoding).error.find("RAW_S16_40M"),
            std::string::npos);
}

TEST(CvbsInputTest, DerivesMetadataPathFromVideoPath) {
  EXPECT_EQ(MetadataPathFor("/m/disc.cvbs"), "/m/disc.meta");
}

TEST(CvbsInputTest, CountsWholeFramesAndReportsATruncatedFrame) {
  constexpr std::int64_t kFrameBytes = ntsc::kSamplesPerFrame * 2;
  const CvbsMetadata unknown_count;

  EXPECT_EQ(CountInputFrames(unknown_count, 3 * kFrameBytes).frames, 3);

  const FrameCountResult truncated =
      CountInputFrames(unknown_count, 3 * kFrameBytes + 10);
  EXPECT_FALSE(truncated.frames.has_value());
  EXPECT_NE(truncated.error.find("input frame 3 is truncated"),
            std::string::npos);
}

TEST(CvbsInputTest, ReportsDisagreementWithDeclaredFrameCount) {
  constexpr std::int64_t kFrameBytes = ntsc::kSamplesPerFrame * 2;
  CvbsMetadata metadata;
  metadata.number_of_sequential_frames = 4;

  EXPECT_NE(CountInputFrames(metadata, 3 * kFrameBytes)
                .error.find("input frame 3 is missing"),
            std::string::npos);
  EXPECT_FALSE(CountInputFrames(metadata, 5 * kFrameBytes).frames.has_value());
  EXPECT_EQ(CountInputFrames(metadata, 4 * kFrameBytes).frames, 4);
}

TEST(CvbsInputTest, ProducesLineAlignedBlocksThatNeverCrossAFrame) {
  constexpr std::int64_t kFrames = 3;
  std::vector<std::uint16_t> words(
      static_cast<std::size_t>(kFrames * ntsc::kSamplesPerFrame), 240);
  // Mark the first sample of frame 1 (stored frame 2 of the file) as white.
  const std::size_t marker =
      static_cast<std::size_t>(2 * ntsc::kSamplesPerFrame);
  words[marker] = 800;
  test::MemoryByteSource source(test::WordsToBytes(words));
  CvbsVideoInput input(&source, SampleEncoding::kU10, 1, 2, 100);

  const VideoInputStream stream = input.Stream();
  EXPECT_EQ(stream.frame_count, 2);
  EXPECT_DOUBLE_EQ(stream.clock.rate_hz, ntsc::kSampleRateHz);
  EXPECT_DOUBLE_EQ(stream.clock.origin_samples, 57.0 / 90.0);

  IreSampleBlock block;
  std::int64_t next = 0;
  std::vector<std::int64_t> sizes;
  double marker_ire = 0.0;
  while (input.ReadBlock(&block)) {
    EXPECT_EQ(block.first_sample, next);
    EXPECT_EQ(block.ire.size() % ntsc::kSamplesPerLine, 0U);
    const std::int64_t end =
        block.first_sample + static_cast<std::int64_t>(block.ire.size());
    // A frame boundary is never inside a block.
    EXPECT_LE(end - (block.first_sample / ntsc::kSamplesPerFrame) *
                        ntsc::kSamplesPerFrame,
              ntsc::kSamplesPerFrame);
    if (block.first_sample == ntsc::kSamplesPerFrame) {
      marker_ire = block.ire[0];
    }
    sizes.push_back(static_cast<std::int64_t>(block.ire.size()));
    next = end;
  }
  EXPECT_EQ(next, 2 * ntsc::kSamplesPerFrame);
  // 525 lines = 5 blocks of 100 lines and one of 25, per frame.
  EXPECT_EQ(sizes.size(), 12U);
  EXPECT_EQ(sizes[5], 25 * ntsc::kSamplesPerLine);
  EXPECT_NEAR(marker_ire, 100.0, kTolerance);
}

TEST(CvbsInputTest, ReportsTruncatedFrameWhileReading) {
  std::vector<std::uint16_t> words(
      static_cast<std::size_t>(ntsc::kSamplesPerFrame) + 100, 240);
  test::MemoryByteSource source(test::WordsToBytes(words));
  CvbsVideoInput input(&source, SampleEncoding::kU10, 0, 2, 525);
  IreSampleBlock block;

  EXPECT_TRUE(input.ReadBlock(&block));
  try {
    input.ReadBlock(&block);
    FAIL() << "expected a truncated-frame error";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("input frame 1 is truncated"),
              std::string::npos);
  }
}

}  // namespace
}  // namespace ld_creator
