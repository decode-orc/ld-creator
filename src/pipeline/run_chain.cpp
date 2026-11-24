/*
 * File:        run_chain.cpp
 * Module:      pipeline
 * Purpose:     Shared plumbing of a run: taps, thread handoffs, hashing
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "ld_creator/pipeline/run_chain.h"

#include "ld_creator/project/resolved_project.h"

namespace ld_creator {

ResolvedRecord HashRunInputs(const PreparedRun& run,
                             const std::string& ld_creator_version,
                             IByteSourceFactory* sources) {
  ResolvedRecord record;
  record.ld_creator_version = ld_creator_version;
  {
    const std::unique_ptr<IByteSource> meta = sources->Open(run.meta_path);
    record.meta_sha256 = HashByteSource(meta.get());
  }
  const std::unique_ptr<IByteSource> video =
      sources->Open(run.config.input.video_path);
  record.cvbs_sha256 = HashByteSource(video.get());
  return record;
}

std::string WriteResolvedProjectFile(const PreparedRun& run,
                                     const ResolvedRecord& record,
                                     IByteSinkFactory* sinks) {
  const std::string path = ResolvedProjectPath(run.config.output.rf_path);
  const std::unique_ptr<IByteSink> resolved = sinks->Create(path);
  WriteText(resolved.get(),
            EmitResolvedProject(run.config, run.frame_count, record));
  resolved->Close();
  return path;
}

}  // namespace ld_creator
