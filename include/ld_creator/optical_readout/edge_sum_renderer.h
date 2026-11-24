/*
 * File:        edge_sum_renderer.h
 * Module:      optical_readout
 * Purpose:     Renders a pit pattern seen through a line-spread kernel
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>
#include <deque>

#include "ld_creator/dsp/edge_response_table.h"
#include "ld_creator/pipeline/pit_edges.h"
#include "ld_creator/pipeline/sampled_signal.h"
#include "ld_creator/playback_mechanics/spot_trajectory.h"

namespace ld_creator {

// What the readout renders: how many samples at Fr, and where the recorded
// area ends (S11 note, "Rendering").
struct ReadoutExtent {
  // Samples n = 0 … sample_count − 1 at τ = n / Fr.
  std::int64_t sample_count = 0;
  // Arc length of the cutting position at the end of the run. The record
  // begins at arc length 0.
  double end_arc_length_m = 0.0;
};

// i(τ) = 1 − m·[½ + ∫ k(x_s(τ) − x)·d(x) dx] for a linear readout with
// kernel k (S11 note, "Rendering"). d = p − ½ is the pit pattern about its
// mean: ±½ inside the record, 0 outside it. d steps by σ = ±1 at each pit
// edge, by p₀ − ½ at the start of the record (p₀ = 1 if the first edge ends
// a pit) and by ½ − p_end at its end. With E the edge response of k,
//
//   i(τ) = 1 − m·[½ + M·d_left + Σ_window Δ_j·E(x_s(τ) − x_j)],
//
// where the window holds the steps within ±W of the spot and d_left is d
// just before it. The kernel's tail beyond ±W is omitted; the S11 note
// bounds the error (the FM pattern has no content at the low spatial
// frequencies where the tail matters).
//
// Streaming: sample n is produced once every edge up to x_s(τ_n) + W is
// known, from the D7 watermark; the rest in Finish(). Block-size
// independent: a sample's value depends only on the steps, never on when it
// is produced. O(edges within ±W) per sample.
//
// Ownership: borrows `response` and `trajectory`, which must outlive the
// renderer. Thread safety: one thread at a time.
class EdgeSumRenderer {
 public:
  EdgeSumRenderer(const EdgeResponseTable* response,
                  const ISpotTrajectory* trajectory, double contrast,
                  ReadoutExtent extent);

  void Process(const ScanBlock& scan, PhotocurrentBlock* output);
  void Finish(PhotocurrentBlock* output);

 private:
  struct Step {
    double arc_length_m;
    double delta;
  };

  void Accept(const ScanBlock& scan);
  // Renders every sample whose window lies below `limit_m`.
  void Render(double limit_m, PhotocurrentBlock* output);
  double SampleAt(double spot_m);

  const EdgeResponseTable* response_;
  const ISpotTrajectory* trajectory_;
  double contrast_;
  ReadoutExtent extent_;
  std::deque<Step> window_;
  double left_ = 0.0;
  double pattern_ = 0.5;
  bool started_ = false;
  double watermark_m_ = 0.0;
  std::int64_t next_sample_ = 0;
};

}  // namespace ld_creator
