/*
 * File:        run_setup.cpp
 * Module:      pipeline
 * Purpose:     Loads a project and checks it against its input media
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/pipeline/run_setup.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <exception>
#include <utility>
#include <vector>

#include "ld_creator/project/project_validator.h"
#include "ld_creator/project/sha256.h"

namespace ld_creator {

namespace {

// Read size for hashing; bounds memory, does not affect the digest.
constexpr std::int64_t kHashChunkBytes = 1 << 20;

constexpr const char* kInputErrorPrefix = "Input error: ";

}  // namespace

std::unique_ptr<IByteSource> FileByteSourceFactory::Open(
    const std::string& path) {
  return std::make_unique<FileByteSource>(path);
}

std::string HashByteSource(IByteSource* source) {
  assert(source != nullptr);
  Sha256 hash;
  std::vector<std::byte> buffer;
  const std::int64_t size = source->Size();
  for (std::int64_t offset = 0; offset < size; offset += kHashChunkBytes) {
    buffer.resize(
        static_cast<std::size_t>(std::min(kHashChunkBytes, size - offset)));
    source->Read(offset, buffer);
    hash.Update(buffer);
  }
  return hash.HexDigest();
}

RunSetupResult PrepareRun(const std::string& project_path,
                          const PathRoots& roots,
                          const std::string& ld_creator_version,
                          const RunSetupServices& services) {
  assert(services.project_reader != nullptr);
  assert(services.metadata_reader != nullptr);
  assert(services.byte_sources != nullptr);

  RunSetupResult result;
  ProjectLoader loader(services.project_reader);
  ProjectLoadResult loaded = loader.Load(project_path, roots);
  result.diagnostics = std::move(loaded.diagnostics);
  if (!loaded.config) {
    return result;
  }
  PreparedRun run;
  run.config = std::move(*loaded.config);
  const std::string& video_path = run.config.input.video_path;
  run.meta_path = MetadataPathFor(video_path);

  const auto fail = [&result](const std::string& message) {
    result.diagnostics.errors.push_back(kInputErrorPrefix + message);
    return std::move(result);
  };

  const CvbsMetadataResult metadata =
      services.metadata_reader->Read(run.meta_path);
  if (!metadata.metadata) {
    return fail(metadata.error);
  }
  const InputFormatResult format = CheckNtscCompositeInput(*metadata.metadata);
  if (!format.encoding) {
    return fail(format.error + " (" + run.meta_path + ")");
  }
  run.encoding = *format.encoding;

  std::unique_ptr<IByteSource> video;
  try {
    video = services.byte_sources->Open(video_path);
  } catch (const std::exception& error) {
    return fail(error.what());
  }
  const FrameCountResult frames =
      CountInputFrames(*metadata.metadata, video->Size());
  if (!frames.frames) {
    return fail(frames.error + " (" + video_path + ")");
  }
  run.file_frame_count = *frames.frames;

  ValidateAgainstInput(run.config, run.file_frame_count, &result.diagnostics);
  if (result.diagnostics.HasErrors()) {
    return result;
  }
  run.frame_count = EffectiveFrameCount(run.config.input, run.file_frame_count);

  if (run.config.resolved) {
    const ResolvedRecord& record = *run.config.resolved;
    if (record.ld_creator_version != ld_creator_version) {
      result.diagnostics.warnings.push_back(
          "Project validation warning: resolved.ld_creator_version = '" +
          record.ld_creator_version + "' differs from this build ('" +
          ld_creator_version + "'); the RF may differ");
    }
    try {
      if (HashByteSource(video.get()) != record.cvbs_sha256) {
        return fail(video_path +
                    " does not match resolved.cvbs_sha256; the input has "
                    "changed since the project was resolved");
      }
      const std::unique_ptr<IByteSource> meta =
          services.byte_sources->Open(run.meta_path);
      if (HashByteSource(meta.get()) != record.meta_sha256) {
        return fail(run.meta_path +
                    " does not match resolved.meta_sha256; the input has "
                    "changed since the project was resolved");
      }
    } catch (const std::exception& error) {
      return fail(error.what());
    }
  }

  result.run = std::move(run);
  return result;
}

}  // namespace ld_creator
