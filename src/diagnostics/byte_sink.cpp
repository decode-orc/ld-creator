/*
 * File:        byte_sink.cpp
 * Module:      diagnostics
 * Purpose:     Byte output for taps, RF and the resolved project
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/diagnostics/byte_sink.h"

#include <cassert>
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace ld_creator {

FileByteSink::FileByteSink(const std::string& path) : path_(path) {
  const std::filesystem::path parent =
      std::filesystem::path(path).parent_path();
  if (!parent.empty()) {
    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
      throw std::runtime_error("cannot create directory " + parent.string() +
                               ": " + error.message());
    }
  }
  stream_.open(path, std::ios::binary | std::ios::trunc);
  if (!stream_) {
    throw std::runtime_error("cannot create " + path);
  }
}

void FileByteSink::Write(std::span<const std::byte> bytes) {
  stream_.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
  if (!stream_) {
    throw std::runtime_error("cannot write " + path_);
  }
}

void FileByteSink::Close() {
  stream_.close();
  if (!stream_) {
    throw std::runtime_error("cannot write " + path_);
  }
}

std::unique_ptr<IByteSink> FileByteSinkFactory::Create(
    const std::string& path) {
  return std::make_unique<FileByteSink>(path);
}

void WriteText(IByteSink* sink, const std::string& text) {
  assert(sink != nullptr);
  sink->Write(std::as_bytes(std::span(text.data(), text.size())));
}

}  // namespace ld_creator
