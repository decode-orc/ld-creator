# Troubleshooting

## "In-source builds are not supported"

CMake was pointed at the source directory as its build directory. Delete the `CMakeCache.txt` file and `CMakeFiles/` directory it created in the source tree, then configure into a separate directory:

```bash
nix develop "path:$PWD" --command cmake -S . -B build -G Ninja
```

## "unknown option", "unknown log level" or "invalid thread count"

ld-creator rejects options it does not recognise. Run `ld-creator --help` for the supported list; see [Command-line options](../reference/cli-options.md). `--threads` takes `auto` or a whole number from 1 to 4096.

## "--project is required"

Every run needs a project file: `ld-creator --project disc.yaml`. Only `--help` and `--version` work without one.

## "Disc modulation is not implemented yet; only output.mode = carrier_diagnostic can be run"

Expected at this stage of development. A disc project (`output.mode: disc`, the default) is checked and then stops, writing nothing. Use `--validate` to run the checks and exit successfully, or set `output.mode: carrier_diagnostic` to write the FM carrier as a [carrier diagnostic](../user-manual/carrier-diagnostic.md).

## "output.mode = carrier_diagnostic: the RF is NOT A DISC"

A warning, logged at the start of every carrier-diagnostic run as a reminder that the file is the bare FM carrier, without any disc stage.

## "N RF samples clipped at the converter's range"

A warning: N samples fell outside the converter's range and were clamped to its end codes. In a carrier-diagnostic run this happens only with `rf_capture.level_fraction` at or very near 1.0, where the carrier's peaks reach full scale; lower it to leave headroom.

## "Run failed: …"

The run stopped part-way: an input file could not be read, or an output file (the RF, a tap or the resolved project) could not be written. The message names the file. Check that the directories exist and are writable and that there is space. The files already written are incomplete and should be discarded.

## Project errors

Every project error names the key path it concerns. All problems are reported in one run.

### `Cannot read project file: …`

The `--project` path does not exist or cannot be read.

### `Project parse error: …`

The file is not valid YAML. The message gives the line and column.

### `… contains unsupported key '…'`

The key is not part of the [project file](../reference/project-file.md) at that level. Check for a typo, or for a key in the wrong block. `video_rate_converter` and `recording_limiter` have no settings, so they have no block.

### `… accepts no keys in this version (reserved for a later phase)`

`recording_signal_combiner`, `disc_replicator` and `playback_mechanics` are reserved. Leave them out, or leave them empty.

### `… is required` / `… block is required`

`input.video_path`, `disc_cutter.format`, `disc_cutter.start_radius_mm`, `disc_cutter.track_pitch_um` and `output.rf_path` have no default. IEC 60857 gives ranges, not nominal values, for the disc's format, radius and pitch, so each project states them.

### `… must be a number` / `must be an integer` / `must be true or false`

The value has the wrong type. Numbers must not be quoted.

### `… is outside the range …`

The value is outside what the model can compute. `allow_out_of_tolerance` does not change ranges.

### `… is outside the IEC 60857 … tolerance …`

The value is outside the standard's tolerance. If that is intended, for example in a sweep, list the key path in `allow_out_of_tolerance`.

### `allow_out_of_tolerance lists …, which is within its IEC tolerance`

A warning, not an error: the listed key is in tolerance, so the entry is not needed. Remove it so a stale entry does not hide a later mistake.

### `allow_out_of_tolerance lists '…', which is not a key with an IEC tolerance`

Only the keys listed under [`allow_out_of_tolerance`](../reference/project-file.md#allow_out_of_tolerance) have a tolerance.

### `disc_cutter.linear_velocity_m_s is required when disc_cutter.format is clv` / `applies to clv only`

A CLV disc needs its linear velocity; a CAV disc turns once per frame and takes none.

### `… the run of N frames ends at radius … mm, which is outside the IEC 60857 §4.12 limit of 145 mm`

The programme would run past the outer edge of the programme area. Start further in, use fewer frames, or a smaller track pitch.

### `optical_readout.hfmi = … is outside the range the linear readout can give`

The readout model reaches its largest HFMI at full pit contrast. The message gives that largest value for the configured optics. Leave `hfmi` out to use it.

### `optical_readout.hfmi = … is outside the IEC 60857 Figure 18 band`

The value is outside the Figure 18 band at the configured track pitch. Figure 18 defines the band only for pitches 1.4–2.0 µm.

### `diagnostics.taps[…].stage = '…' does not run with output.mode = carrier_diagnostic`

A carrier-diagnostic run stops after `fm_phase_modulator`, so later stages produce nothing to tap. Tap `cvbs_input`, `video_rate_converter`, `group_delay_predistorter`, `video_pre_emphasis` or `fm_phase_modulator`, or remove the tap.

### `… uses an unknown path root`

Only `{project}` and `{output}` exist.

## Input errors

### `Input error: cannot open metadata …`

The `.meta` beside the `.cvbs` is missing or is not an SQLite database. It must have the same base name as the `.cvbs`. If `--output-root` is relative, remember that it is relative to the project file's directory.

### `Input error: input video standard is '…'; only NTSC is supported`

Only NTSC recordings are accepted so far.

### `Input error: input signal_type is …` / `signal_state_preset is …` / `sample_encoding_preset is …`

The recording must be composite, time-base stable at 4fsc, in a 4fsc encoding; see [Input files](../reference/input-files.md).

### `Input error: input frame N is truncated` / `is missing`

The `.cvbs` ends partway through a frame, or holds fewer frames than its metadata declares. Regenerate the recording.

### `input.frame_count = … runs beyond the input's … frames` / `diagnostics.taps[…] frames … are beyond the run's … frames`

The requested frames are not all in the file, or a tap's frames are not all in the run.

### `Input error: … does not match resolved.cvbs_sha256` (or `meta_sha256`)

A resolved project names input that has changed since it was written, so it would not reproduce its RF. Regenerate the input with the same videosynth project, or run the original project instead.

### `resolved.ld_creator_version = '…' differs from this build`

A warning: the resolved project was written by a different ld-creator build, so its RF may differ.

## "cannot open log output"

The path given to `--log-file` could not be created. Check that the directory exists and is writable.
