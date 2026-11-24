/*
 * File:        edge_sum_renderer.cpp
 * Module:      optical_readout
 * Purpose:     Renders a pit pattern seen through a line-spread kernel
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/optical_readout/edge_sum_renderer.h"

#include <algorithm>
#include <cassert>
#include <limits>

#include "ld_creator/optical_readout/readout_kernel.h"

namespace ld_creator {

namespace {

// A sample waits until the watermark is this far beyond its window. It
// covers rounding in the watermark's conversion from arc length to playback
// time and back (far below 1 nm), and costs only latency.
constexpr double kWatermarkMarginM = 1.0e-9;

// p = ½ + d: d is +½ on a pit and −½ on land.
constexpr double kPatternMean = 0.5;

}  // namespace

EdgeSumRenderer::EdgeSumRenderer(const EdgeResponseTable* response,
                                 const ISpotTrajectory* trajectory,
                                 double contrast, ReadoutExtent extent)
    : response_(response),
      trajectory_(trajectory),
      contrast_(contrast),
      extent_(extent) {
  assert(response_ != nullptr);
  assert(trajectory_ != nullptr);
}

void EdgeSumRenderer::Accept(const ScanBlock& scan) {
  for (const PitEdge& edge : scan.edges) {
    if (!started_) {
      // The pattern before the first edge is the state that edge leaves.
      const double first_state =
          edge.polarity == EdgePolarity::kPitEnd ? 1.0 : 0.0;
      // The record starts at arc length 0 (or at the first edge, if a pit
      // length bias moves that edge before it).
      window_.push_back({.arc_length_m = std::min(0.0, edge.arc_length_m),
                         .delta = first_state - kPatternMean});
      pattern_ = first_state;
      started_ = true;
    }
    const double delta = edge.polarity == EdgePolarity::kPitStart ? 1.0 : -1.0;
    assert(window_.empty() || edge.arc_length_m >= window_.back().arc_length_m);
    window_.push_back({.arc_length_m = edge.arc_length_m, .delta = delta});
    pattern_ += delta;
  }
  watermark_m_ =
      std::max(watermark_m_, trajectory_->ArcLengthAt(scan.complete_until_s));
}

void EdgeSumRenderer::Process(const ScanBlock& scan,
                              PhotocurrentBlock* output) {
  Accept(scan);
  output->first_sample = next_sample_;
  output->level.clear();
  if (!started_) {
    return;
  }
  // Windows that reach the end of the record wait for Finish(), which adds
  // the closing step.
  Render(std::min(watermark_m_, extent_.end_arc_length_m) - kWatermarkMarginM,
         output);
}

void EdgeSumRenderer::Finish(PhotocurrentBlock* output) {
  output->first_sample = next_sample_;
  output->level.clear();
  if (started_) {
    window_.push_back({.arc_length_m = extent_.end_arc_length_m,
                       .delta = kPatternMean - pattern_});
  }
  // With no edges at all, d = 0 everywhere: the mean level.
  Render(std::numeric_limits<double>::infinity(), output);
}

void EdgeSumRenderer::Render(double limit_m, PhotocurrentBlock* output) {
  const double reach = response_->HalfWidth();
  while (next_sample_ < extent_.sample_count) {
    const double tau = static_cast<double>(next_sample_) / kReadoutRateHz;
    const double spot = trajectory_->ArcLengthAt(tau);
    if (!(spot + reach < limit_m)) {
      break;
    }
    output->level.push_back(SampleAt(spot));
    ++next_sample_;
  }
}

double EdgeSumRenderer::SampleAt(double spot_m) {
  const EdgeResponseTable& response = *response_;
  const double reach = response.HalfWidth();
  // Fold steps the window has passed into d_left.
  while (!window_.empty() && window_.front().arc_length_m <= spot_m - reach) {
    left_ += window_.front().delta;
    window_.pop_front();
  }
  double sum = response.Mass() * left_;
  for (const Step& step : window_) {
    const double offset = spot_m - step.arc_length_m;
    if (offset <= -reach) {
      break;
    }
    sum += step.delta * response(offset);
  }
  return 1.0 - (contrast_ * (kPatternMean + sum));
}

}  // namespace ld_creator
