# Project file schema

The project file is YAML and is the complete description of a run: the
input, every stage parameter, every seed and the output. For the same
project file and the same input media, ld-creator is to write byte-identical
RF on any host, at any thread count.

The key-by-key definition (types, defaults, ranges, IEC tolerances, path
roots, the resolved project) is the published reference,
[docs/reference/project-file.md](../../../docs/reference/project-file.md),
and it is implemented: the parser, validator and resolved-project writer in
`src/project/`. This note keeps what the reference does not: how the schema
relates to the model, and the keys reserved for later phases.

## Relation to the parameter register

- Every key's default is the stage's ideal setting from its
  [stage note](README.md), and every key has a row in the
  [parameter register](parameter-register.md). Every IEC tolerance in the
  validator is a register tolerance; the ranges are the values the model can
  compute.
- `input`, `disc_cutter` and `output` are required. The standard gives
  ranges, not nominal values, for the disc's format, radius and pitch, so
  there is no documented default for them ([S8](s08-disc-cutter.md)).
- `optical_readout.hfmi` has a computed default: the largest value with pit
  contrast m ≤ 1 at the configured optics,
  `(4/π)·H(802 pits/mm)` = 0.27142 at nominal optics
  ([S11](s11-optical-readout.md)).
- The Figure 18 HFMI band is read from the figure at 1.4 and 2.0 µm and
  taken as linear in pitch between (register `S11.hfmi_limits`). Outside
  1.4–2.0 µm the figure gives no band, so `hfmi` is out of tolerance there.
- The disc-cutter tolerance on `start_radius_mm` (§4.12, the run ends at
  ≤ 145 mm) depends on the input's frame count, so it is checked when the
  input is opened, with the S8 radius equations.
- Implementation constants that are part of the model, such as the modelling
  rate `Fm`, are fixed in the code, listed as `fixed` in the register, and
  covered by `resolved.ld_creator_version`.

## Decisions in the resolved project

- **Absolute paths.** The resolved project lives beside the RF, not beside
  the original project, so `{project}` would mean a different directory when
  it is run. Paths are therefore written resolved and absolute; resolving the
  resolved project again then gives the same bytes.
- **Effective frame count.** `input.frame_count` is written as the number of
  frames read, so the resolved project does not depend on the file's length.
- **Exact numbers.** Doubles are written in shortest round-trip form, so
  every value, including the computed `hfmi`, reads back bit for bit.

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

The three reserved blocks are accepted when empty; any key in them is an
error until the phase that adds it.

## Examples

- [examples/ideal-ntsc-cav.yaml](examples/ideal-ntsc-cav.yaml): the ideal CAV
  disc with only the required blocks.
- [examples/all-keys.yaml](examples/all-keys.yaml): every key, at its default
  where it has one.

Both are validated by the functional test
`tests/functional/test_example_projects.cpp`.
