/*
 * File:        tap_writer.h
 * Module:      diagnostics
 * Purpose:     Writes diagnostic taps: sampled data or edge lists
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "ld_creator/diagnostics/byte_sink.h"
#include "ld_creator/pipeline/pit_edges.h"
#include "ld_creator/pipeline/sampled_signal.h"

namespace ld_creator {

// What a tap holds, written into its descriptor
// (docs/reference/diagnostic-taps.md).
struct TapIdentity {
  std::string stage;           // stage module name, e.g. "cvbs_input"
  std::string representation;  // "D1" … "D10"
  std::string unit;            // e.g. "IRE", "rad", "land = 1"
  std::int64_t first_frame = 0;
  std::int64_t frame_count = 0;
};

// The data file and descriptor paths for tap base path `base_path`.
std::string TapSampleDataPath(const std::string& base_path);
std::string TapEdgeDataPath(const std::string& base_path);
std::string TapDescriptorPath(const std::string& base_path);

// Writes the stream samples with index in [begin_sample, end_sample) as
// little-endian IEEE 754 float64 to "<base>.f64", and on Finish() a YAML
// descriptor to "<base>.tap.yaml". Blocks may be passed in any split; samples
// outside the range are ignored.
//
// Thread safety: one thread at a time. Ownership: borrows `factory` during
// construction and Finish().
class SampledTapWriter {
 public:
  SampledTapWriter(IByteSinkFactory* factory, std::string base_path,
                   TapIdentity identity, SampleClock clock,
                   std::int64_t begin_sample, std::int64_t end_sample);

  // Writes the in-range part of `samples`, whose first element is stream
  // sample `first_sample`.
  void Write(std::int64_t first_sample, std::span<const double> samples);

  void Finish();

 private:
  IByteSinkFactory* factory_;
  std::string base_path_;
  TapIdentity identity_;
  SampleClock clock_;
  std::int64_t begin_sample_;
  std::int64_t end_sample_;
  std::int64_t written_ = 0;
  std::unique_ptr<IByteSink> data_;
};

// Fields of an edge record, in order. Each is float64 except the last,
// polarity, which is int8 (+1 pit start, −1 pit end).
enum class EdgeRecordLayout {
  // D4: time_s, polarity (9 bytes).
  kRecordedEdge,
  // D5: arc_length_m, radius_m, polarity (17 bytes).
  kPitEdge,
};

// Writes packed little-endian edge records to "<base>.edges" and on Finish()
// a YAML descriptor. Only edges whose first field (time for D4, arc length
// for D5) lies in [range_begin, range_end) are written.
//
// Thread safety and ownership as SampledTapWriter.
class EdgeTapWriter {
 public:
  EdgeTapWriter(IByteSinkFactory* factory, std::string base_path,
                TapIdentity identity, EdgeRecordLayout layout,
                double range_begin, double range_end);

  void Write(std::span<const RecordedEdge> edges);
  void Write(std::span<const PitEdge> edges);
  void Finish();

 private:
  IByteSinkFactory* factory_;
  std::string base_path_;
  TapIdentity identity_;
  EdgeRecordLayout layout_;
  double range_begin_;
  double range_end_;
  std::int64_t written_ = 0;
  std::unique_ptr<IByteSink> data_;
};

}  // namespace ld_creator
