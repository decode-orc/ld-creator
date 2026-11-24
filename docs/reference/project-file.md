# Project File

An ld-creator run is described by a YAML **project file**: the input, every stage parameter and the output. Nothing that affects the RF comes from the command line or the environment, so the project file and the media it names determine the RF.

!!! warning "Partly implemented"
    ld-creator reads and checks project files, and checks them against their input (`--validate`). It runs projects with [`output.mode: carrier_diagnostic`](#output). Disc modulation (`output.mode: disc`, the default) is not implemented yet: such a run stops after the checks. The keys of later stages are accepted and checked now so that projects written today stay valid.

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

These follow videosynth's project file.

- **Strict keys.** A key that is not listed here is an error, at every level, as is a key given twice.
- **Ideal defaults.** An omitted block or key takes the default in its table: the stage's ideal setting.
- **Required blocks.** `input`, `disc_cutter` and `output`. IEC 60857 gives ranges, not nominal values, for the disc's format, radius and track pitch, so these have no default.
- **Types.** Numbers are plain YAML numbers: a quoted `"320"` is a string and is rejected. Booleans are `true` or `false`. Integers have no fractional part.
- **Units in key names.** `_hz`, `_ns`, `_mm`, `_um`, `_m_s`, `_rad`. Frequencies are in Hz.
- **Quoted versions.** Quote `project.version`, so that `1.10` is not read as a number.
- **Range and tolerance.** Each numeric key has a **range**, the values the model can compute; outside it is always an error. Where the standard sets one, it also has an **IEC tolerance**; outside it is an error unless the key path is listed in [`allow_out_of_tolerance`](#allow_out_of_tolerance).

### Paths

A path may start with a root:

| Root | Stands for |
|------|------------|
| `{project}` | The directory of the project file |
| `{output}` | `--output-root`, or the project file's directory when it is not given |

A path without a root is relative to the project file's directory; an absolute path is used as is. A relative `--output-root` is itself relative to the project file's directory, as in videosynth. Any other `{name}` root is an error.

## Blocks

| Block | Required | Stage | Contents |
|-------|----------|-------|----------|
| [`project`](#project) | No | — | Identity |
| [`input`](#input) | Yes | S1 | The videosynth recording |
| [`group_delay_predistorter`](#group_delay_predistorter) | No | S3 | IEC 60857 §9.1.7 group-delay pre-distortion |
| [`video_pre_emphasis`](#video_pre_emphasis) | No | S4 | IEC 60857 §9.2.4 pre-emphasis |
| [`fm_phase_modulator`](#fm_phase_modulator) | No | S5 | IEC 60857 §9.2 video carrier |
| [`disc_cutter`](#disc_cutter) | Yes | S8 | Disc format and geometry |
| [`optical_readout`](#optical_readout) | No | S11 | IEC 60857 §11 readout optics |
| [`player_front_end`](#player_front_end) | No | S12 | Player RF path |
| [`rf_capture`](#rf_capture) | No | S13 | Capture device |
| [`output`](#output) | Yes | S14 | RF file and mode |
| [`diagnostics`](#diagnostics) | No | — | Diagnostic taps |
| [`allow_out_of_tolerance`](#allow_out_of_tolerance) | No | — | Key paths allowed outside their IEC tolerance |
| [`resolved`](#the-resolved-project) | No | — | Written by ld-creator into a resolved project |

The video rate converter (S2) and recording limiter (S7) have no settings and no block. `recording_signal_combiner` (S6), `disc_replicator` (S9) and `playback_mechanics` (S10) are reserved: they may be present but empty, and any key in them is an error until the stages gain settings.

## `project`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `name` | string | empty | A project name |
| `version` | string | empty | The project's own version; quote it |
| `description` | string | empty | Free text |

None of these affects the output.

## `input`

| Key | Type | Required | Default | Range |
|-----|------|----------|---------|-------|
| `video_path` | string | Yes | — | A `.cvbs` file. Its `.meta` is the same path with `.cvbs` replaced by `.meta` |
| `first_frame` | integer | No | 0 | ≥ 0, and less than the file's frame count |
| `frame_count` | integer | No | every frame from `first_frame` | ≥ 1, and within the file |

The recording must be NTSC composite video at 4fsc; see [Input files](input-files.md).

## `group_delay_predistorter`

| Key | Type | Default | Range | IEC tolerance |
|-----|------|---------|-------|---------------|
| `enabled` | bool | `true` | — | `false` is outside tolerance: IEC 60857 §9.1.7 says the pre-distortion "shall" be applied |
| `delay_ns` | list of `[f_mhz, ns]` | `[[0.5, 0], [2.0, -15], [3.0, -45], [3.58, -80], [4.0, -135], [4.2, -200]]` | at least two points, the first `[0.5, 0]`, frequencies ascending within 0.5–4.2 MHz | IEC 60857 §9.1.7, Figure 8: the six table frequencies, with delays within ±15 ns at 2, 3 and 3.58 MHz, ±30 ns at 4 MHz and ±50 ns at 4.2 MHz |

Delays are relative to 0.5 MHz. Between the table points the delay follows a smooth curve through them, which stays within the Figure 8 band; below 0.5 MHz it is 0, and above the last point it holds the last delay. The filter is designed from this table each time the stage is built.

## `video_pre_emphasis`

| Key | Type | Default | Range | IEC tolerance |
|-----|------|---------|-------|---------------|
| `t1_ns` | float | 320 | 50–1000, and greater than `t2_ns` | 312–328 (IEC 60857 §9.2.4) |
| `t2_ns` | float | 120 | 20–500 | 118–122 (IEC 60857 §9.2.4) |

## `fm_phase_modulator`

| Key | Type | Default | Range | IEC tolerance |
|-----|------|---------|-------|---------------|
| `blanking_hz` | float | 8 100 000 | 6 000 000–10 000 000 | 8 050 000–8 150 000 (IEC 60857 §9.2.2) |
| `deviation_hz` | float | 1 700 000 | 500 000–3 000 000 | 1 665 000–1 735 000 (IEC 60857 §9.2.3) |
| `initial_phase_rad` | float | 0.0 | −π to π | — |

`deviation_hz` is the carrier's swing from sync tip (−40 IRE) to white (+100 IRE).

## `disc_cutter`

| Key | Type | Required | Range | IEC tolerance |
|-----|------|----------|-------|---------------|
| `format` | string | Yes | `cav`, `clv` | — |
| `start_radius_mm` | float | Yes | 41.5–150 | the run must end at a radius of 145 mm or less (IEC 60857 §4.12) |
| `track_pitch_um` | float | Yes | 0.5–5.0 | 1.4–2.0 (IEC 60857 §4.15) |
| `linear_velocity_m_s` | float | CLV only | 5–20 | 10.1–11.4 (IEC 60857 §4.7.2) |

`start_radius_mm` is the radius at 0H of the first input frame. The radius at the end of the run is checked against the input's frame count:

| Format | Radius after N frames |
|--------|-----------------------|
| `cav` | r₀ + N·p (one revolution per frame, IEC 60857 §4.7.1) |
| `clv` | √(r₀² + p·v·t/π), with t = N / (30000/1001 Hz) |

`linear_velocity_m_s` is required for `clv` and is an error for `cav`.

## `optical_readout`

| Key | Type | Default | Range | IEC tolerance |
|-----|------|---------|-------|---------------|
| `numerical_aperture` | float | 0.40 | 0.30–0.60 | 0.39–0.41 (IEC 60857 §11.1.2) |
| `rim_intensity` | float | 0.23 | 0.05–1.0 | 0.22–0.24 (IEC 60857 §11.1.3) |
| `hfmi` | float | the largest value the optics allow (0.27142 at the default optics) | above 0, and at most the largest value the optics allow | inside the IEC 60857 Figure 18 band at `disc_cutter.track_pitch_um` |

The readout model gives HFMI = m·(4/π)·H(802 pits/mm), where H is the optics' transfer function and m the pit contrast, which cannot exceed 1. The default is m = 1. The Figure 18 band runs from 0.16–0.40 at 1.4 µm to 0.18–0.42 at 2.0 µm, linearly in between; at other pitches it is undefined, so `hfmi` is out of tolerance there. The resolved project records the computed default.

## `player_front_end`

| Key | Type | Default | Values |
|-----|------|---------|--------|
| `profile` | string | `iec_reference` | `iec_reference`: flat, unity gain, noiseless (IEC 60857 §11.2) |
| `mtf_equaliser` | string | `none` | `none`: the unequalised signal at a player's RF test point, as real captures carry. `inverse`: a diagnostic that removes the readout's frequency response up to the optical cut-off; decode it with ld-decode `-m 0` |

## `rf_capture`

| Key | Type | Default | Range |
|-----|------|---------|-------|
| `profile` | string | `domesday_duplicator` | `domesday_duplicator`, `ideal` |
| `level_fraction` | float | 0.75 | 0.05–1.0, the reference level as a fraction of the converter's full scale |

## `output`

| Key | Type | Required | Default | Values |
|-----|------|----------|---------|--------|
| `rf_path` | string | Yes | — | A path ending in `.s16` |
| `mode` | string | No | `disc` | `disc`, `carrier_diagnostic` |

`carrier_diagnostic` writes the FM carrier directly, with no disc stages: the stages up to `fm_phase_modulator` run, and the carrier `cos φ` is sampled at 40 MHz and quantised by the `rf_capture` profile at `level_fraction` of full scale. It is **not a disc**; the resolved project records the mode and says so in its header, so the file cannot be mistaken for one. See [Carrier diagnostic](../user-manual/carrier-diagnostic.md).

`disc` is not implemented yet.

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
| `taps[].stage` | string | Yes | — | `cvbs_input`, `video_rate_converter`, `group_delay_predistorter`, `video_pre_emphasis`, `fm_phase_modulator`, `recording_limiter`, `disc_cutter`, `optical_readout`, `player_front_end` or `rf_capture` |
| `taps[].path` | string | Yes | — | Base path; each tap needs its own |
| `taps[].first_frame` | integer | No | 0 | First frame to write, counted from `input.first_frame` |
| `taps[].frame_count` | integer | No | 1 | Frames to write; the range must lie within the run |

Taps never change the RF. Their file formats are in [Diagnostic taps](diagnostic-taps.md).

With `output.mode: carrier_diagnostic`, only the stages that run can be tapped: `cvbs_input`, `video_rate_converter`, `group_delay_predistorter`, `video_pre_emphasis` and `fm_phase_modulator`. A tap on a later stage is an error.

## `allow_out_of_tolerance`

```yaml
allow_out_of_tolerance:
  - video_pre_emphasis.t1_ns
  - group_delay_predistorter.enabled
```

Key paths whose values may lie outside their IEC tolerance, but still inside their range. Each must be one of the keys with a tolerance above:

`group_delay_predistorter.enabled`, `group_delay_predistorter.delay_ns`, `video_pre_emphasis.t1_ns`, `video_pre_emphasis.t2_ns`, `fm_phase_modulator.blanking_hz`, `fm_phase_modulator.deviation_hz`, `disc_cutter.start_radius_mm`, `disc_cutter.track_pitch_um`, `disc_cutter.linear_velocity_m_s`, `optical_readout.numerical_aperture`, `optical_readout.rim_intensity`, `optical_readout.hfmi`.

Listing any other path, or a path twice, is an error. Listing a key whose value is within tolerance is a warning, so a stale entry is noticed.

## The resolved project

Each run writes `<rf_path>` with `.s16` replaced by `.resolved.yaml` (today, carrier-diagnostic runs only). It is a valid project file that:

- contains every block and key, with its effective value, in the order of this page;
- gives every path as an absolute path, `input.frame_count` as the number of frames read, and `optical_readout.hfmi` as its computed value;
- writes numbers in their shortest exact form, so every value reads back bit for bit;
- ends with a `resolved:` block.

| Key | Description |
|-----|-------------|
| `resolved.ld_creator_version` | The ld-creator build that wrote it |
| `resolved.cvbs_sha256` | SHA-256 of the `.cvbs` |
| `resolved.meta_sha256` | SHA-256 of the `.meta` |
| `resolved.rf_sha256` | SHA-256 of the RF file |

Loading a project with a `resolved:` block checks it: if the `.cvbs` or `.meta` no longer matches its hash, that is an error naming the file; a different `ld_creator_version` is a warning, because the RF may differ. Running a resolved project writes the same RF, and the same resolved project, byte for byte.

## What is not in the project

`--project`, `--validate`, `--output-root`, `--threads`, `--log-level` and `--log-file` cannot change the output, so they stay on the [command line](cli-options.md). Implementation constants of the model, such as its internal sample rates, are fixed in the code and covered by `resolved.ld_creator_version`.
