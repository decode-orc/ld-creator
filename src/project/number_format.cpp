/*
 * File:        number_format.cpp
 * Module:      project
 * Purpose:     Exact, locale-independent text forms of numbers
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/project/number_format.h"

#include <array>
#include <charconv>
#include <cmath>
#include <locale>
#include <sstream>
#include <string>

namespace ld_creator {

std::string FormatDouble(double value) {
  // 32 characters hold any shortest round-trip double.
  std::array<char, 32> buffer{};
  const std::to_chars_result result =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  return {buffer.data(), result.ptr};
}

std::optional<double> ParseDouble(std::string_view text) {
  // YAML allows a leading '+'; the checks below do not.
  if (!text.empty() && text.front() == '+') {
    text.remove_prefix(1);
  }
  if (text.empty()) {
    return std::nullopt;
  }
  // A digit or sign must start the number; this also rejects leading
  // whitespace, which the stream would skip.
  const char first = text.front();
  if (first != '-' && first != '.' && (first < '0' || first > '9')) {
    return std::nullopt;
  }
  // std::from_chars for double is unavailable on older macOS deployment
  // targets; a classic-locale stream gives the same locale independence.
  std::istringstream stream{std::string(text)};
  stream.imbue(std::locale::classic());
  double value = 0.0;
  stream >> value;
  if (stream.fail() || stream.peek() != std::char_traits<char>::eof() ||
      !std::isfinite(value)) {
    return std::nullopt;
  }
  return value;
}

}  // namespace ld_creator
