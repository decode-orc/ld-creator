/*
 * File:        disc_run.h
 * Module:      pipeline
 * Purpose:     Runs S1–S14: the RF of a modelled disc, with taps
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstdint>
#include <string>

#include "ld_creator/pipeline/run_chain.h"
#include "ld_creator/pipeline/run_setup.h"

namespace ld_creator {

// Samples a run of `frame_count` frames gives at the readout rate Fr and at
// the capture rate: the n with n / rate < frame_count frame periods, so a
// disc run and a carrier-diagnostic run of the same input have the same
// length. With M = frame_count · Fm samples per frame:
//   Fr: ⌈M·176/189⌉;   40 MHz: ⌈M·88/189⌉.
std::int64_t ReadoutSampleCount(std::int64_t frame_count);
std::int64_t CaptureSampleCount(std::int64_t frame_count);

// Runs a project whose output.mode is disc: S1 → S2 → … → S13 → S14, with
// the disc type and start radius from disc_cutter. Writes the RF to
// output.rf_path, every declared tap, and the resolved project beside the
// RF. Uses up to `threads` threads; the output does not depend on the
// number.
//
// Throws std::runtime_error on an input or output failure.
RfRunResult RunDisc(const PreparedRun& run,
                    const std::string& ld_creator_version, unsigned int threads,
                    const RunOutputServices& services);

}  // namespace ld_creator
