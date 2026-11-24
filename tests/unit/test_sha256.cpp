/*
 * File:        test_sha256.cpp
 * Module:      project
 * Purpose:     Unit tests for SHA-256 against the FIPS 180-4 example vectors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <span>
#include <string>

#include "ld_creator/project/sha256.h"

namespace ld_creator {
namespace {

std::string Digest(const std::string& text, std::size_t piece) {
  Sha256 hash;
  for (std::size_t offset = 0; offset < text.size(); offset += piece) {
    const std::string part = text.substr(offset, piece);
    hash.Update(std::as_bytes(std::span(part.data(), part.size())));
  }
  return hash.HexDigest();
}

TEST(Sha256Test, MatchesKnownDigestOfEmptyInput) {
  EXPECT_EQ(Digest("", 1),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256Test, MatchesFipsOneBlockExample) {
  EXPECT_EQ(Digest("abc", 3),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256Test, MatchesFipsTwoBlockExampleInAnySplit) {
  const std::string text =
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  const std::string expected =
      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1";
  for (const std::size_t piece : {1U, 7U, 55U, 56U, 64U}) {
    EXPECT_EQ(Digest(text, piece), expected) << piece;
  }
}

TEST(Sha256Test, MatchesMillionCharacterExample) {
  EXPECT_EQ(Digest(std::string(1000000, 'a'), 4096),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

}  // namespace
}  // namespace ld_creator
