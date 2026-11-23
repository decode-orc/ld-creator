# Project file schema

The ld-creator project file is YAML. It is the complete description of a run:
the input, every stage parameter, every seed and the output. For the same
project file and the same input media, ld-creator writes byte-identical RF on
any host, at any thread count.

This is a design note. Nothing here is implemented until the HLD says so.
When it is, this content becomes `docs/reference/project-file.md` and its
per-block pages (plan Task 2.5).

```yaml
project:
  name: NtscCavInner
  version: "1.0"
  description: Ideal NTSC CAV disc at the inner programme radius

input:
  video_path: "{output}/ntsc_cav.cvbs"

disc_cutter:
  format: cav
  start_radius_mm: 55.0
  track_pitch_um: 1.6667

output:
  rf_path: "{output}/ntsc_cav_inner.s16"
```

## Conventions

These follow videosynth's project file, so the two tools read alike.

- **Strict keys.** Any key not listed here is a parse error, at every level.
- **Ideal defaults.** An omitted block, or an omitted key, takes the default
  in its table. Every default is the stage's ideal setting from its
  [stage note](README.md), and every default has a row in the
  [parameter register](parameter-register.md).
- **Required blocks.** `input`, `disc_cutter` and `output` are required. The
  standard gives ranges, not nominal values, for the disc's format, radius
  and pitch, so there is no documented default for them
  ([S8](s08-disc-cutter.md)).
- **Units in key names.** `_hz`, `_ns`, `_mm`, `_um`, `_m_s`, `_rad`.
  Frequencies in Hz, as in videosynth.
- **Path roots.** Paths may start with `{project}` (the project file's
  directory) or `{output}` (`--output-root`, else the project file's
  directory), with the same resolution rules as videosynth.
- **Quoted versions.** `project.version` is a string; quote it.
- **Range and tolerance.** Each numeric key has a **range** (values the model
  can compute) and, where the standard sets one, an **IEC tolerance**. Outside
  the range is always an error. Outside the IEC tolerance is an error unless
  the key path is listed in `allow_out_of_tolerance`.
- **Errors name the key path**, for example
  `Project validation error: video_pre_emphasis.t1_ns = 340 is outside the IEC 60857 §9.2.4 tolerance 312–328`.

## Top-level blocks

| Block | Required | Stage | Description |
|-------|----------|-------|-------------|
| `project` | No | — | Identity (as videosynth) |
| `input` | Yes | S1 | The videosynth recording |
| `group_delay_predistorter` | No | S3 | §9.1.7 pre-distortion |
| `video_pre_emphasis` | No | S4 | §9.2.4 pre-emphasis |
| `fm_phase_modulator` | No | S5 | §9.2 carrier |
| `disc_cutter` | Yes | S8 | Disc format and geometry |
| `optical_readout` | No | S11 | §11 readout |
| `player_front_end` | No | S12 | Player RF path |
| `rf_capture` | No | S13 | Capture device |
| `output` | Yes | S14 | RF file and mode |
| `diagnostics` | No | — | Diagnostic taps |
| `allow_out_of_tolerance` | No | — | Key paths allowed outside IEC tolerance |

S2 (`video_rate_converter`) and S7 (`recording_limiter`) have no settings.
`recording_signal_combiner` (S6), `disc_replicator` (S9) and
`playback_mechanics` (S10) are reserved names. They accept no keys until the
phases that add their settings, so in this scope any content in them is an
error.

## `project`

| Key | Type | Required | Description |
|-----|------|----------|-------------|
| `name` | string | No | A project name |
| `version` | string | No | The project's own version; quote it |
| `description` | string | No | Free text |

None of these affects the output.

## `input`

| Key | Type | Required | Default | Range |
|-----|------|----------|---------|-------|
| `video_path` | string | Yes | — | A `.cvbs` path. The `.meta` path is derived as videosynth derives it |
| `first_frame` | integer | No | 0 | ≥ 0, less than the file's frame count |
| `frame_count` | integer | No | all frames from `first_frame` | ≥ 1, within the file |

The `.meta` must declare the `NTSC` video standard and a supported sample
encoding ([S1](s01-cvbs-input.md)).

## `group_delay_predistorter`

| Key | Type | Default | Range | IEC tolerance |
|-----|------|---------|-------|---------------|
| `enabled` | bool | `true` | — | `false` is outside tolerance (§9.1.7 "shall") |
| `delay_ns` | list of `[f_mhz, ns]` | `[[0.5, 0], [2.0, -15], [3.0, -45], [3.58, -80], [4.0, -135], [4.2, -200]]` | frequencies ascending in 0.5–4.2 MHz; first point `[0.5, 0]` | each point within its §9.1.7 tolerance; frequencies must be the six table frequencies |

## `video_pre_emphasis`

| Key | Type | Default | Range | IEC tolerance |
|-----|------|---------|-------|---------------|
| `t1_ns` | float | 320 | 50–1000, > `t2_ns` | 312–328 |
| `t2_ns` | float | 120 | 20–500 | 118–122 |

## `fm_phase_modulator`

| Key | Type | Default | Range | IEC tolerance |
|-----|------|---------|-------|---------------|
| `blanking_hz` | float | 8 100 000 | 6 000 000–10 000 000 | 8 050 000–8 150 000 |
| `deviation_hz` | float | 1 700 000 | 500 000–3 000 000 | 1 665 000–1 735 000 |
| `initial_phase_rad` | float | 0.0 | −π to π | — |

## `disc_cutter`

| Key | Type | Required | Range | IEC tolerance |
|-----|------|----------|-------|---------------|
| `format` | string | Yes | `cav`, `clv` | — |
| `start_radius_mm` | float | Yes | 41.5–150 | the radius at the last frame ≤ 145 (§4.12) |
| `track_pitch_um` | float | Yes | 0.5–5.0 | 1.4–2.0 (§4.15) |
| `linear_velocity_m_s` | float | CLV only | 5–20 | 10.1–11.4 (§4.7.2) |

`start_radius_mm` is the radius at 0H of the first input frame.

## `optical_readout`

| Key | Type | Default | Range | IEC tolerance |
|-----|------|---------|-------|---------------|
| `numerical_aperture` | float | 0.40 | 0.30–0.60 | 0.39–0.41 |
| `rim_intensity` | float | 0.23 | 0.05–1.0 | 0.22–0.24 |
| `hfmi` | float | the largest value with pit contrast m ≤ 1 (0.272 at nominal optics) | > 0, with m ≤ 1 | inside the Figure 18 band at `disc_cutter.track_pitch_um` |

The default `hfmi` depends on the optics keys, so it is computed during
validation. The resolved project records the computed value.

## `player_front_end`

| Key | Type | Default | Values |
|-----|------|---------|--------|
| `profile` | string | `iec_reference` | `iec_reference` |
| `mtf_equaliser` | string | `none` | `none`, `inverse` |

`none` gives the unequalised test-point signal that real captures carry, and
ld-decode compensates the MTF itself. `inverse` is a diagnostic. It removes
the S11 MTF up to the optical cut-off and is decoded with `-m 0`
([S12](s12-player-front-end.md)). Like `carrier_diagnostic`, it is recorded
in the resolved project, so the file is not mistaken for a disc capture.

## `rf_capture`

| Key | Type | Default | Range |
|-----|------|---------|-------|
| `profile` | string | `domesday_duplicator` | `domesday_duplicator`, `ideal` |
| `level_fraction` | float | 0.75 | 0.05–1.0 |

## `output`

| Key | Type | Required | Default | Values |
|-----|------|----------|---------|--------|
| `rf_path` | string | Yes | — | A path ending in `.s16` |
| `mode` | string | No | `disc` | `disc`, `carrier_diagnostic` |

`carrier_diagnostic` writes `cos φ` from S5, sampled through the S13 profile
and the S14 writer, with no disc stages. It is **not a disc**: the resolved
project records `mode: carrier_diagnostic` so the file cannot be mistaken
for one.

The resolved project is written to `rf_path` with `.s16` replaced by
`.resolved.yaml`. Its path is not configurable, as videosynth's `.meta` path
is not.

## `diagnostics`

```yaml
diagnostics:
  taps:
    - stage: fm_phase_modulator
      path: "{output}/taps/phase"
      first_frame: 10
      frame_count: 1
```

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `taps` | list | Yes | — | One entry per tap |
| `taps[].stage` | string | Yes | — | A stage module name that has a tap (see each stage note) |
| `taps[].path` | string | Yes | — | Base path; the writer adds the data and descriptor suffixes |
| `taps[].first_frame` | integer | No | 0 | First frame (relative to `input.first_frame`) to write |
| `taps[].frame_count` | integer | No | 1 | Frames to write |

Taps never change the RF. The tap formats are defined in plan Task 2.3.

## `allow_out_of_tolerance`

```yaml
allow_out_of_tolerance:
  - video_pre_emphasis.t1_ns
  - group_delay_predistorter.enabled
```

A list of key paths whose values may lie outside their IEC tolerance (but
still inside their range). Each listed path must exist in the schema and must
have an IEC tolerance; listing any other path is an error. Listing a key whose
value is in tolerance is a warning, so a stale entry is noticed. The list is
copied into the resolved project.

## Reserved for later phases

| Block or key | Phase | Purpose |
|--------------|-------|---------|
| `recording_signal_combiner.*` | audio, EFM | PWM sources and levels |
| `disc_cutter` exposure keys | 8 | cutting-spot profile, resist threshold |
| `disc_replicator.*` | 8 | pit geometry and replication fidelity; disc defects |
| `playback_mechanics.*` | 8 | eccentricity, time-base error, focus, tilt |
| `<stage>.seed` | 9 | a separate seed for each stage with a random process |
| `optical_readout.model` | 10 | `linear` or `phase_pit` |
| `player_front_end.profile` values | 9 | real player responses |

## The resolved project

Every run writes a resolved project beside the RF. It is a valid project file
that:

- contains every block and every key, with its effective value, in the order
  of this document;
- writes computed defaults (such as `optical_readout.hfmi`) as values;
- adds a `resolved:` block with the ld-creator version, the input media's
  SHA-256 hashes (`.cvbs` and `.meta`) and the output RF's SHA-256;
- reproduces the RF byte for byte when run.

The `resolved:` block is accepted on input and checked: a hash mismatch is an
error naming the file, and a version mismatch is a warning. Loading a resolved
project and resolving it again gives byte-identical YAML (plan Task 2.3).

## What is not in the project

These cannot change the output, so they stay on the command line:
`--project`, `--validate`, `--output-root`, `--threads`, `--log-level` and
`--log-file`. Implementation constants that are part of the model, such as
the modelling rate `Fm`, are fixed in the code, listed as `fixed` in the
register, and covered by the version in the resolved project.

## Examples

- [examples/ideal-ntsc-cav.yaml](examples/ideal-ntsc-cav.yaml): the ideal CAV
  disc with only the required blocks.
- [examples/all-keys.yaml](examples/all-keys.yaml): every key, at its default
  where it has one.
