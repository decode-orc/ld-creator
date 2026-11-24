/*
 * File:        tap_writer.cpp
 * Module:      diagnostics
 * Purpose:     Writes diagnostic taps: sampled data or edge lists
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/diagnostics/tap_writer.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <utility>

#include "ld_creator/project/number_format.h"

namespace ld_creator {

namespace {

// Descriptor format identifier and version (docs/reference/diagnostic-taps.md).
constexpr const char* kDescriptorFormat = "ld-creator-tap";
constexpr int kDescriptorVersion = 1;

constexpr std::size_t kFloat64Bytes = 8;

void AppendFloat64(double value, std::vector<std::byte>* bytes) {
  const auto bits = std::bit_cast<std::uint64_t>(value);
  for (std::size_t i = 0; i < kFloat64Bytes; ++i) {
    bytes->push_back(static_cast<std::byte>(bits >> (8 * i)));
  }
}

void AppendInt8(std::int8_t value, std::vector<std::byte>* bytes) {
  bytes->push_back(static_cast<std::byte>(value));
}

std::string FileName(const std::string& path) {
  return std::filesystem::path(path).filename().string();
}

std::string DescriptorHeader(const TapIdentity& identity, const char* kind,
                             const std::string& data_path) {
  std::string text;
  text += std::string("format: ") + kDescriptorFormat + "\n";
  text += "format_version: " + std::to_string(kDescriptorVersion) + "\n";
  text += "stage: " + identity.stage + "\n";
  text += "representation: " + identity.representation + "\n";
  text += std::string("kind: ") + kind + "\n";
  text += "data_file: \"" + FileName(data_path) + "\"\n";
  text += "unit: \"" + identity.unit + "\"\n";
  text += "first_frame: " + std::to_string(identity.first_frame) + "\n";
  text += "frame_count: " + std::to_string(identity.frame_count) + "\n";
  return text;
}

void WriteDescriptor(IByteSinkFactory* factory, const std::string& base_path,
                     const std::string& text) {
  const std::unique_ptr<IByteSink> sink =
      factory->Create(TapDescriptorPath(base_path));
  WriteText(sink.get(), text);
  sink->Close();
}

}  // namespace

std::string TapSampleDataPath(const std::string& base_path) {
  return base_path + ".f64";
}

std::string TapEdgeDataPath(const std::string& base_path) {
  return base_path + ".edges";
}

std::string TapDescriptorPath(const std::string& base_path) {
  return base_path + ".tap.yaml";
}

SampledTapWriter::SampledTapWriter(IByteSinkFactory* factory,
                                   std::string base_path, TapIdentity identity,
                                   SampleClock clock, std::int64_t begin_sample,
                                   std::int64_t end_sample)
    : factory_(factory),
      base_path_(std::move(base_path)),
      identity_(std::move(identity)),
      clock_(clock),
      begin_sample_(begin_sample),
      end_sample_(end_sample) {
  assert(factory_ != nullptr);
  assert(end_sample_ >= begin_sample_);
  data_ = factory_->Create(TapSampleDataPath(base_path_));
}

void SampledTapWriter::Write(std::int64_t first_sample,
                             std::span<const double> samples) {
  const std::int64_t block_end =
      first_sample + static_cast<std::int64_t>(samples.size());
  const std::int64_t from = std::max(first_sample, begin_sample_);
  const std::int64_t to = std::min(block_end, end_sample_);
  if (from >= to) {
    return;
  }
  std::vector<std::byte> bytes;
  bytes.reserve(static_cast<std::size_t>(to - from) * kFloat64Bytes);
  for (std::int64_t index = from; index < to; ++index) {
    AppendFloat64(samples[static_cast<std::size_t>(index - first_sample)],
                  &bytes);
  }
  data_->Write(bytes);
  written_ += to - from;
}

void SampledTapWriter::Finish() {
  data_->Close();
  std::string text =
      DescriptorHeader(identity_, "sampled", TapSampleDataPath(base_path_));
  text += "sample_type: float64_le\n";
  text += "rate_hz: " + FormatDouble(clock_.rate_hz) + "\n";
  text += "first_sample: " + std::to_string(begin_sample_) + "\n";
  text +=
      "time_of_first_sample_s: " + FormatDouble(clock_.TimeOf(begin_sample_)) +
      "\n";
  text += "sample_count: " + std::to_string(written_) + "\n";
  WriteDescriptor(factory_, base_path_, text);
}

EdgeTapWriter::EdgeTapWriter(IByteSinkFactory* factory, std::string base_path,
                             TapIdentity identity, EdgeRecordLayout layout,
                             double range_begin, double range_end)
    : factory_(factory),
      base_path_(std::move(base_path)),
      identity_(std::move(identity)),
      layout_(layout),
      range_begin_(range_begin),
      range_end_(range_end) {
  assert(factory_ != nullptr);
  data_ = factory_->Create(TapEdgeDataPath(base_path_));
}

void EdgeTapWriter::Write(std::span<const RecordedEdge> edges) {
  assert(layout_ == EdgeRecordLayout::kRecordedEdge);
  std::vector<std::byte> bytes;
  for (const RecordedEdge& edge : edges) {
    if (edge.time_s >= range_begin_ && edge.time_s < range_end_) {
      AppendFloat64(edge.time_s, &bytes);
      AppendInt8(static_cast<std::int8_t>(edge.polarity), &bytes);
      ++written_;
    }
  }
  if (!bytes.empty()) {
    data_->Write(bytes);
  }
}

void EdgeTapWriter::Write(std::span<const PitEdge> edges) {
  assert(layout_ == EdgeRecordLayout::kPitEdge);
  std::vector<std::byte> bytes;
  for (const PitEdge& edge : edges) {
    if (edge.arc_length_m >= range_begin_ && edge.arc_length_m < range_end_) {
      AppendFloat64(edge.arc_length_m, &bytes);
      AppendFloat64(edge.radius_m, &bytes);
      AppendInt8(static_cast<std::int8_t>(edge.polarity), &bytes);
      ++written_;
    }
  }
  if (!bytes.empty()) {
    data_->Write(bytes);
  }
}

void EdgeTapWriter::Finish() {
  data_->Close();
  std::string text =
      DescriptorHeader(identity_, "edges", TapEdgeDataPath(base_path_));
  if (layout_ == EdgeRecordLayout::kRecordedEdge) {
    text += "record_bytes: 9\n";
    text += "fields:\n";
    text += "  - {name: time_s, type: float64_le}\n";
    text += "  - {name: polarity, type: int8}\n";
  } else {
    text += "record_bytes: 17\n";
    text += "fields:\n";
    text += "  - {name: arc_length_m, type: float64_le}\n";
    text += "  - {name: radius_m, type: float64_le}\n";
    text += "  - {name: polarity, type: int8}\n";
  }
  text += "range_begin: " + FormatDouble(range_begin_) + "\n";
  text += "range_end: " + FormatDouble(range_end_) + "\n";
  text += "record_count: " + std::to_string(written_) + "\n";
  WriteDescriptor(factory_, base_path_, text);
}

}  // namespace ld_creator
