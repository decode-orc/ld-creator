/*
 * File:        carrier_diagnostic_run.h
 * Module:      pipeline
 * Purpose:     Runs S1–S5 and the carrier diagnostic, writing RF and taps
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <string>

#include "ld_creator/pipeline/run_chain.h"
#include "ld_creator/pipeline/run_setup.h"

namespace ld_creator {

// Runs a project whose output.mode is carrier_diagnostic: S1 → S2 → S3 → S4
// → S5 → carrier diagnostic renderer → S14. Writes the RF to
// output.rf_path, every declared tap, and the resolved project beside the
// RF (with the SHA-256 of the .cvbs, the .meta and the RF written). Uses up
// to `threads` threads; the output does not depend on the number.
//
// Throws std::runtime_error on an input or output failure.
RfRunResult RunCarrierDiagnostic(const PreparedRun& run,
                                 const std::string& ld_creator_version,
                                 unsigned int threads,
                                 const RunOutputServices& services);

}  // namespace ld_creator
