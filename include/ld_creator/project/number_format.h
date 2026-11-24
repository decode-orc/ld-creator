/*
 * File:        number_format.h
 * Module:      project
 * Purpose:     Exact, locale-independent text forms of numbers
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ld_creator {

// The shortest decimal text that reads back as exactly `value` (std::to_chars
// shortest round-trip form). Used for the resolved project, so that loading
// it reproduces every double bit for bit. Locale-independent.
std::string FormatDouble(double value);

// Parses the whole of `text` as a finite double. Locale-independent.
std::optional<double> ParseDouble(std::string_view text);

}  // namespace ld_creator
