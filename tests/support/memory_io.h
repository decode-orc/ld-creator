/*
 * File:        memory_io.h
 * Module:      tests
 * Purpose:     In-memory byte sinks and sources for unit tests
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ld_creator/cvbs_input/cvbs_video_input.h"
#include "ld_creator/diagnostics/byte_sink.h"

namespace ld_creator::test {

// Collects the bytes written to each path.
class MemorySinkFactory final : public IByteSinkFactory {
 public:
  class Sink final : public IByteSink {
   public:
    explicit Sink(std::vector<std::byte>* bytes) : bytes_(bytes) {}
    void Write(std::span<const std::byte> bytes) override {
      bytes_->insert(bytes_->end(), bytes.begin(), bytes.end());
    }
    void Close() override { closed = true; }
    bool closed = false;

   private:
    std::vector<std::byte>* bytes_;
  };

  std::unique_ptr<IByteSink> Create(const std::string& path) override {
    files[path].clear();
    return std::make_unique<Sink>(&files[path]);
  }

  [[nodiscard]] std::string Text(const std::string& path) const {
    const std::vector<std::byte>& bytes = files.at(path);
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
  }

  std::map<std::string, std::vector<std::byte>> files;
};

// A byte source over a buffer held in memory.
class MemoryByteSource final : public IByteSource {
 public:
  explicit MemoryByteSource(std::vector<std::byte> bytes)
      : bytes_(std::move(bytes)) {}
  [[nodiscard]] std::int64_t Size() const override {
    return static_cast<std::int64_t>(bytes_.size());
  }
  void Read(std::int64_t offset, std::span<std::byte> bytes) override {
    if (offset < 0 ||
        offset + static_cast<std::int64_t>(bytes.size()) > Size()) {
      throw std::runtime_error("short read");
    }
    std::copy_n(bytes_.begin() + offset, bytes.size(), bytes.begin());
  }

 private:
  std::vector<std::byte> bytes_;
};

// Little-endian bytes of 16-bit words.
inline std::vector<std::byte> WordsToBytes(
    const std::vector<std::uint16_t>& words) {
  std::vector<std::byte> bytes;
  for (const std::uint16_t word : words) {
    bytes.push_back(static_cast<std::byte>(word & 0xFF));
    bytes.push_back(static_cast<std::byte>(word >> 8));
  }
  return bytes;
}

}  // namespace ld_creator::test
