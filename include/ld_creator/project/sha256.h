/*
 * File:        sha256.h
 * Module:      project
 * Purpose:     SHA-256 digests for the resolved project
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace ld_creator {

// Incremental SHA-256 (FIPS 180-4). Feed data with Update() in any number of
// pieces; the digest depends only on the concatenation.
//
// Thread safety: an instance is used by one thread at a time.
class Sha256 {
 public:
  Sha256();

  void Update(std::span<const std::byte> data);

  // Completes the digest and returns it as 64 lower-case hex digits. The
  // object must not be updated afterwards.
  std::string HexDigest();

 private:
  void Compress(const std::uint8_t* block);

  std::array<std::uint32_t, 8> state_{};
  std::array<std::uint8_t, 64> buffer_{};
  std::size_t buffered_ = 0;
  std::uint64_t total_bytes_ = 0;
};

}  // namespace ld_creator
