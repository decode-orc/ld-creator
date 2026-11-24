/*
 * File:        byte_sink.h
 * Module:      diagnostics
 * Purpose:     Byte output for taps, RF and the resolved project
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <cstddef>
#include <fstream>
#include <memory>
#include <span>
#include <string>

namespace ld_creator {

// A destination for bytes. Throws std::runtime_error on a write failure.
// Used by one thread at a time.
class IByteSink {
 public:
  virtual ~IByteSink() = default;
  virtual void Write(std::span<const std::byte> bytes) = 0;
  // Flushes and closes; no Write() follows.
  virtual void Close() = 0;
};

// Opens byte sinks by path. Injected so that writers can be unit tested
// without touching the filesystem.
class IByteSinkFactory {
 public:
  virtual ~IByteSinkFactory() = default;
  // Creates (or truncates) the file at `path`. Throws std::runtime_error if
  // it cannot be created.
  virtual std::unique_ptr<IByteSink> Create(const std::string& path) = 0;
};

// Writes a file, creating its parent directories.
class FileByteSink final : public IByteSink {
 public:
  explicit FileByteSink(const std::string& path);
  void Write(std::span<const std::byte> bytes) override;
  void Close() override;

 private:
  std::string path_;
  std::ofstream stream_;
};

class FileByteSinkFactory final : public IByteSinkFactory {
 public:
  std::unique_ptr<IByteSink> Create(const std::string& path) override;
};

// Writes `text` to `sink` as bytes.
void WriteText(IByteSink* sink, const std::string& text);

}  // namespace ld_creator
