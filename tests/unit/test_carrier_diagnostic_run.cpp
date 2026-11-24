/*
 * File:        test_carrier_diagnostic_run.cpp
 * Module:      tests
 * Purpose:     Unit tests for the carrier-diagnostic run, with in-memory files
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "ld_creator/pipeline/carrier_diagnostic_run.h"
#include "ld_creator/project/sha256.h"
#include "ld_creator/standards/ntsc_timing.h"
#include "ld_creator/video_rate_converter/video_rate_converter.h"
#include "memory_io.h"

namespace ld_creator {
namespace {

class MemoryByteSources final : public IByteSourceFactory {
 public:
  std::unique_ptr<IByteSource> Open(const std::string& path) override {
    const auto it = files.find(path);
    if (it == files.end()) {
      throw std::runtime_error("cannot open " + path);
    }
    return std::make_unique<test::MemoryByteSource>(it->second);
  }
  std::map<std::string, std::vector<std::byte>> files;
};

// One NTSC frame of CVBS_S16_4FSC words (w = 32·(v − 240)): each line has
// 67 samples of sync tip (code 16) and then a mid-grey level (code 520).
std::vector<std::byte> OneFrame() {
  std::vector<std::uint16_t> words;
  for (std::int64_t line = 0; line < ntsc::kLinesPerFrame; ++line) {
    for (std::int64_t sample = 0; sample < ntsc::kSamplesPerLine; ++sample) {
      const int code = sample < 67 ? 16 : 520;
      words.push_back(static_cast<std::uint16_t>((code - 240) * 32));
    }
  }
  return test::WordsToBytes(words);
}

std::string Hash(const std::vector<std::byte>& bytes) {
  Sha256 hash;
  hash.Update(bytes);
  return hash.HexDigest();
}

class CarrierDiagnosticRunTest : public ::testing::Test {
 protected:
  CarrierDiagnosticRunTest() {
    sources_.files["/in/in.cvbs"] = OneFrame();
    sources_.files["/in/in.meta"] = std::vector<std::byte>(32, std::byte{7});
    run_.config.input.video_path = "/in/in.cvbs";
    run_.config.disc_cutter.start_radius_mm = 55.0;
    run_.config.disc_cutter.track_pitch_um = 1.6667;
    run_.config.output.rf_path = "/out/carrier.s16";
    run_.config.output.mode = OutputMode::kCarrierDiagnostic;
    run_.config.optical_readout.hfmi = 0.25;
    run_.meta_path = "/in/in.meta";
    run_.encoding = SampleEncoding::kS16;
    run_.file_frame_count = 1;
    run_.frame_count = 1;
  }

  CarrierDiagnosticResult RunDiagnostic(test::MemorySinkFactory* sinks,
                                        unsigned int threads) {
    return RunCarrierDiagnostic(
        run_, "build1", threads,
        {.byte_sources = &sources_, .byte_sinks = sinks});
  }

  MemoryByteSources sources_;
  PreparedRun run_;
};

TEST_F(CarrierDiagnosticRunTest,
       WritesTheRfAndAResolvedProjectFlaggedNotADisc) {
  test::MemorySinkFactory sinks;
  const CarrierDiagnosticResult result = RunDiagnostic(&sinks, 1);

  // 40 MHz × 1001/30000 s = 1 334 666.7 → 1 334 667 samples, 2 bytes each.
  EXPECT_EQ(result.rf_samples, 1334667);
  EXPECT_EQ(sinks.files.at("/out/carrier.s16").size(), 2U * 1334667U);
  EXPECT_EQ(result.clipped_samples, 0);
  EXPECT_EQ(result.rf_sha256, Hash(sinks.files.at("/out/carrier.s16")));
  EXPECT_EQ(result.resolved_path, "/out/carrier.resolved.yaml");

  const std::string resolved = sinks.Text("/out/carrier.resolved.yaml");
  EXPECT_NE(resolved.find("# NOT A DISC"), std::string::npos);
  EXPECT_NE(resolved.find("mode: carrier_diagnostic"), std::string::npos);
  EXPECT_NE(resolved.find(result.rf_sha256), std::string::npos);
  EXPECT_NE(resolved.find(Hash(sources_.files.at("/in/in.cvbs"))),
            std::string::npos);
  EXPECT_NE(resolved.find(Hash(sources_.files.at("/in/in.meta"))),
            std::string::npos);
}

TEST_F(CarrierDiagnosticRunTest, CodesStayInsideTheConfiguredLevel) {
  test::MemorySinkFactory sinks;
  RunDiagnostic(&sinks, 1);
  const std::vector<std::byte>& bytes = sinks.files.at("/out/carrier.s16");
  // DdD words are (c − 512)·64; level 0.75 keeps c within 512 ± 384.
  for (std::size_t i = 0; i < bytes.size(); i += 2) {
    const auto word = static_cast<std::int16_t>(
        static_cast<std::uint16_t>(bytes[i]) |
        (static_cast<std::uint16_t>(bytes[i + 1]) << 8));
    ASSERT_EQ(word % 64, 0);
    const int code = (word / 64) + 512;
    ASSERT_GE(code, 512 - 384);
    ASSERT_LE(code, 512 + 384);
  }
}

TEST_F(CarrierDiagnosticRunTest, RfAndTapsDoNotDependOnThreadCount) {
  run_.config.taps = {{.stage = "cvbs_input",
                       .path = "/taps/d1",
                       .first_frame = 0,
                       .frame_count = 1},
                      {.stage = "fm_phase_modulator",
                       .path = "/taps/d3",
                       .first_frame = 0,
                       .frame_count = 1}};
  test::MemorySinkFactory one;
  RunDiagnostic(&one, 1);
  EXPECT_EQ(one.files.at("/taps/d1.f64").size(),
            8U * static_cast<std::size_t>(ntsc::kSamplesPerFrame));
  EXPECT_EQ(one.files.at("/taps/d3.f64").size(),
            8U * static_cast<std::size_t>(kModellingSamplesPerFrame));
  EXPECT_NE(one.Text("/taps/d3.tap.yaml").find("representation: D3"),
            std::string::npos);
  for (const unsigned int threads : {3U, 8U}) {
    test::MemorySinkFactory many;
    RunDiagnostic(&many, threads);
    EXPECT_EQ(many.files, one.files) << threads;
  }
}

TEST_F(CarrierDiagnosticRunTest, ReportsAMissingInput) {
  sources_.files.erase("/in/in.cvbs");
  test::MemorySinkFactory sinks;
  EXPECT_THROW(RunDiagnostic(&sinks, 1), std::runtime_error);
}

}  // namespace
}  // namespace ld_creator
