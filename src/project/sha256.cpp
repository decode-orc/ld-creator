/*
 * File:        sha256.cpp
 * Module:      project
 * Purpose:     SHA-256 digests for the resolved project
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/project/sha256.h"

#include <algorithm>
#include <bit>

namespace ld_creator {

namespace {

// FIPS 180-4 §4.2.2: SHA-256 round constants.
constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

// FIPS 180-4 §5.3.3: initial hash value.
constexpr std::array<std::uint32_t, 8> kInitialState = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

constexpr std::size_t kBlockBytes = 64;
constexpr std::size_t kLengthBytes = 8;

}  // namespace

Sha256::Sha256() : state_(kInitialState) {}

void Sha256::Compress(const std::uint8_t* block) {
  std::array<std::uint32_t, 64> w{};
  for (std::size_t i = 0; i < 16; ++i) {
    w[i] = (static_cast<std::uint32_t>(block[4 * i]) << 24) |
           (static_cast<std::uint32_t>(block[4 * i + 1]) << 16) |
           (static_cast<std::uint32_t>(block[4 * i + 2]) << 8) |
           static_cast<std::uint32_t>(block[4 * i + 3]);
  }
  for (std::size_t i = 16; i < 64; ++i) {
    const std::uint32_t s0 =
        std::rotr(w[i - 15], 7) ^ std::rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const std::uint32_t s1 =
        std::rotr(w[i - 2], 17) ^ std::rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  std::uint32_t a = state_[0];
  std::uint32_t b = state_[1];
  std::uint32_t c = state_[2];
  std::uint32_t d = state_[3];
  std::uint32_t e = state_[4];
  std::uint32_t f = state_[5];
  std::uint32_t g = state_[6];
  std::uint32_t h = state_[7];
  for (std::size_t i = 0; i < 64; ++i) {
    const std::uint32_t s1 =
        std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
    const std::uint32_t choose = (e & f) ^ (~e & g);
    const std::uint32_t t1 = h + s1 + choose + kRoundConstants[i] + w[i];
    const std::uint32_t s0 =
        std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
    const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    const std::uint32_t t2 = s0 + majority;
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }
  state_[0] += a;
  state_[1] += b;
  state_[2] += c;
  state_[3] += d;
  state_[4] += e;
  state_[5] += f;
  state_[6] += g;
  state_[7] += h;
}

void Sha256::Update(std::span<const std::byte> data) {
  total_bytes_ += data.size();
  std::size_t offset = 0;
  while (offset < data.size()) {
    const std::size_t take =
        std::min(kBlockBytes - buffered_, data.size() - offset);
    for (std::size_t i = 0; i < take; ++i) {
      buffer_[buffered_ + i] = static_cast<std::uint8_t>(data[offset + i]);
    }
    buffered_ += take;
    offset += take;
    if (buffered_ == kBlockBytes) {
      Compress(buffer_.data());
      buffered_ = 0;
    }
  }
}

std::string Sha256::HexDigest() {
  // FIPS 180-4 §5.1.1: append 0x80, zeros, then the bit length (big-endian).
  const std::uint64_t total_bits = total_bytes_ * 8;
  buffer_[buffered_++] = 0x80;
  if (buffered_ > kBlockBytes - kLengthBytes) {
    std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_),
              buffer_.end(), 0);
    Compress(buffer_.data());
    buffered_ = 0;
  }
  std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffered_),
            buffer_.end() - kLengthBytes, 0);
  for (std::size_t i = 0; i < kLengthBytes; ++i) {
    buffer_[kBlockBytes - 1 - i] =
        static_cast<std::uint8_t>(total_bits >> (8 * i));
  }
  Compress(buffer_.data());

  constexpr const char* kHexDigits = "0123456789abcdef";
  std::string hex;
  hex.reserve(64);
  for (const std::uint32_t word : state_) {
    for (int shift = 28; shift >= 0; shift -= 4) {
      hex.push_back(kHexDigits[(word >> shift) & 0xF]);
    }
  }
  return hex;
}

}  // namespace ld_creator
