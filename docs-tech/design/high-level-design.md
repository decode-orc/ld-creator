# ld-creator High-Level Design

Living design specification for ld-creator. Keep it aligned with the code
(AGENTS.md §9.5). Each section states whether it is **implemented** or
**designed**; designed sections describe intent, not available behaviour.

---

## 1. Purpose

ld-creator converts a CVBS recording and its associated media into the RF
signal a LaserVision player would recover from a disc mastered to
IEC 60856 (PAL) or IEC 60857 (NTSC).

It does this by modelling the life of a real disc as a pipeline of physical
stages: signal processing at the mastering facility, the laser beam
recorder, replication, the player's mechanics, optics and front end, and the
capture device (§3).

It is one stage of a known-truth measurement loop for ld-decode:

```
            truth                          RF                        decoded
videosynth ───────► ld-creator ─────────────────────► ld-decode ───────────► compare
 (CVBS, WAV, EFM)   (this repo)          (.s16)                      (CVBS, WAV, EFM)
     │                                                                          ▲
     └──────────────────────────────── truth ───────────────────────────────────┘
```

Because videosynth's output is exact and every ld-creator stage follows a
cited source, any difference between the truth and ld-decode's output is
attributable to the decode chain, or to a documented ld-creator modelling
choice (each recorded in the stage notes and the parameter register,
[../user/disc-model/](../user/disc-model/README.md)).

### 1.1 Scope and non-goals

In scope:

- The physical disc chain, from the video signal to the captured RF samples,
  stage by stage (§3).
- **Disc and RF-domain impairments**: effects of mastering, replication,
  disc mechanics, readout optics, the player front end and the capture
  device, each added as a separate, deterministic, documented stage setting.
  This includes **physical dropouts** (disc defects), which belong to
  ld-creator as an RF-domain impairment so that a decoder's RF dropout
  detection can be measured.

Non-goals:

- Modelling a specific player's servo or tracking dynamics. Only their
  residual effects within the limits of IEC 60856/60857 §4 and §12 are in
  scope.
- **CVBS-domain impairments.** These stay in videosynth, which puts them in
  the truth. Round-trip inputs do not use videosynth's `laserdisc.channel`
  impairments or its CVBS dropouts, so that disc effects come only from
  ld-creator.
- Decoding. ld-creator never demodulates RF (the analysis scripts in
  `scripts/` estimate frequency for verification only).

---

## 2. External interfaces

### 2.1 Project file — *implemented (loading and validation)*

A run is described by a **YAML project file**, as in videosynth. It holds
every input, every stage parameter, every seed and the output, so the
project file and the media it names determine the RF byte for byte. The
key-by-key reference is `docs/reference/project-file.md`; the design notes
behind it are in
[../user/disc-model/project-file-schema.md](../user/disc-model/project-file-schema.md).

Implemented in `src/project/`: the yaml-cpp parser (`project_parser.cpp`),
range, tolerance and allow-list checks (`project_validator.cpp`), path roots
and loading through an injected `ITextFileReader` (`project_loader.cpp`), and
the resolved-project writer (`resolved_project.cpp`, with `sha256.cpp`).
`src/pipeline/run_setup.cpp` checks a loaded project against its input
media (frame ranges, the §4.12 end radius, resolved-project hashes).

- Each pipeline stage has its own block. An omitted block or key takes the
  stage's documented ideal value.
- `input`, `disc_cutter` and `output` are required. The standard gives ranges,
  not nominal values, for disc format, radius and pitch, so these have no
  default.
- Parsing is strict. Unknown keys and out-of-range values are errors. Values
  outside an IEC tolerance are errors unless their key path is listed in
  `allow_out_of_tolerance`.

### 2.2 Inputs — *implemented for NTSC composite video*

All inputs follow the CVBS file format specification
(`docs-tech/cvbs-file-format-specification/docs/index.md`).

| Input | File(s) | Source spec |
|-------|---------|-------------|
| Composite video | `<base>.cvbs` + `<base>.meta` (SQLite) | CVBS spec, "Video Data Format", "Metadata Schema" |
| Analogue audio | `<base>_audio_<pair>.wav` (48 kHz / 24-bit stereo), `audio_channel_pair` table | CVBS spec, "Audio Data" |
| EFM digital audio | `<base>.efm` (T3–T11 t-value stream) + `<base>.efm.meta` | CVBS spec, `extensions/efm-extension-format.md` |

- The video standard (PAL / NTSC) is taken from the `.meta` video standard
  preset; ld-creator does not accept a conflicting override. NTSC is the first
  standard implemented: `src/cvbs_input/` reads the `.meta`
  (`cvbs_metadata.cpp`) and the samples (`cvbs_video_input.cpp`, S1), and
  accepts only NTSC composite at 4fsc with a time-base-stable signal state.
  Audio and EFM inputs are designed.
- Video sample encodings: `CVBS_U10_4FSC`, `CVBS_U16_4FSC`, `CVBS_S16_4FSC` and
  `CVBS_TPG21_4FSC`. Values below sync tip (the LaserVision VBI burst) are
  carried through.
- Y/C (`.cvbsy`/`.cvbsc`) input is out of scope: a LaserDisc records composite.
- The EFM extension's t-value stream is the natural input to the EFM pulse
  generator — no CIRC or EFM *encoding* is needed in ld-creator; videosynth
  already produces the channel-bit-level t-values.
- Audio channel pair 1 maps to analogue channels I (left) and II (right).

### 2.3 Outputs — *designed (formats implemented, not yet written by a run)*

| Output | Description |
|--------|-------------|
| `<name>.s16` | Signed 16-bit little-endian mono RF at 40 MHz, ld-decode's default rate. With the default DomesdayDuplicator capture profile, each word is a 10-bit ADC code *c* stored as (c − 512) × 64, exactly as an uncompressed DomesdayDuplicator capture |
| `<name>.resolved.yaml` | The **resolved project**: every key with its effective value, the ld-creator version, and SHA-256 hashes of the input media and the RF. Running it reproduces the RF byte for byte |
| Diagnostic taps | Optional per-stage data declared in the project's `diagnostics:` block. They never change the RF |

`.lds` and `.ldf` output may be added later; ld-decode reads `.s16` directly.

The `.s16` word mapping (`src/rf_sample_writer/`, S14), the resolved-project
text (`src/project/resolved_project.cpp`) and the tap formats
(`src/diagnostics/tap_writer.cpp`, documented in
`docs/reference/diagnostic-taps.md`) are implemented and unit tested. A run
writes none of them until the pipeline is wired (plan Task 5.4).

### 2.4 Command line — *implemented*

`src/main.cpp`, `src/cli/command_line.cpp`. Options: `--project`,
`--validate`, `--output-root`, `--threads`, `--log-level`, `--log-file`,
`--help`, `--version`, matching videosynth. No command-line option may change
the output. User reference: `docs/reference/cli-options.md`.

A run loads and checks the project and its input; with `--validate` it then
exits successfully. Without it, it reports that RF modulation is not
implemented and exits with a failure status.

---

## 3. Signal model — *designed*

### 3.1 Pipeline

Each stage consumes one documented data representation and produces the
next. Each has a stage note under
[../user/disc-model/](../user/disc-model/README.md) that cites the source of
every equation and value, and a row in the parameter register for every
parameter.

| # | Stage (module) | Physical process | Status |
|---|----------------|------------------|--------|
| S1 | `cvbs_input` | Programme source: CVBS to IRE at 4fsc | implemented |
| S2 | `video_rate_converter` | Resampling to the modelling rate | designed |
| S3 | `group_delay_predistorter` | §9.1.7 group-delay pre-distortion | designed |
| S4 | `video_pre_emphasis` | §9.2.4 pre-emphasis | designed |
| S5 | `fm_phase_modulator` | §9.2 FM: carrier phase | designed |
| S6 | `recording_signal_combiner` | §7.2.1 / Amd 2 §13.1.5 PWM of audio and EFM (video only: pass-through) | designed |
| S7 | `recording_limiter` | Binary cut: pit-edge times | designed |
| S8 | `disc_cutter` | Laser beam recorder: time → track position (§4.7, §4.15) | designed |
| S9 | `disc_replicator` | Master → stamper → pressed disc | designed |
| S10 | `playback_mechanics` | Spindle and tracking: playback time → track position | designed |
| S11 | `optical_readout` | §11.1 reference optics: pits → photocurrent | designed (transfer function H implemented, used for the `hfmi` default) |
| S12 | `player_front_end` | Photodiode, preamplifier (§11.2); diagnostic MTF equaliser | designed |
| S13 | `rf_capture` | Capture device (DomesdayDuplicator by default) | designed |
| S14 | `rf_sample_writer` | `.s16` and resolved project | implemented (writer; not yet wired) |

Each stage has an `I`-prefixed streaming interface
(`include/ld_creator/pipeline/stage_interfaces.h`) over the typed D1–D10
blocks (`sampled_signal.h`, `pit_edges.h`). A stage's output depends only
on the concatenation of its input, never on the block split.
`block_chain.h` connects stages, optionally across `ThreadedHandoff`
thread boundaries that preserve order, so the output is identical for any
block size and thread count (unit tested with fake stages).

Pit edges are carried as exact event lists from S7 to S10, and the readout
(S11) turns them back into a waveform at an 80 MHz readout rate, above twice
the optical cut-off at every radius. With every stage at its ideal setting,
the pipeline describes a disc nominal in every IEC parameter, read by the
IEC §11 reference system and captured by a noiseless DomesdayDuplicator.

### 3.2 Video path

S1 → S5. Parameters per standard:

| Parameter | PAL (IEC 60856) | NTSC (IEC 60857) |
|-----------|-----------------|------------------|
| Modulation | FM, positive (§9.2.1) | FM, positive (§9.2.1) |
| Blanking-level carrier | 7.1 MHz ± 50 kHz (§9.2.2) | 8.1 MHz ± 50 kHz (§9.2.2) |
| Deviation | Blanking → +100 % white: 800 ± 20 kHz (§9.2.3) | Sync tip (−40 IRE) → white (+100 IRE): 1.7 MHz ± 35 kHz (§9.2.3) |
| Pre-emphasis | §9.2.4, Figure 12 | §9.2.4, Figure 9: (1 + jωt₁)/(1 + jωt₂), t₁ = 320 ± 8 ns, t₂ = 120 ± 2 ns |
| Group-delay pre-distortion | §9.1.6, Figure 11 (0.5–4.8 MHz) | §9.1.7, Figure 8 (0.5–4.2 MHz) |
| Pilot burst | 3.75 MHz (240 fH) on sync level (§9.1.2) — present in the CVBS from videosynth | — |

The NTSC values and their derivations are in the S3–S5 notes.

### 3.3 Analogue audio path

```
WAV (48 kHz) → [optional CX compression, §8.4/App. B] → pre-emphasis (§8.3) → resample → FM subcarrier → PWM in S6
```

| Parameter | PAL (IEC 60856 §8) | NTSC (IEC 60857 §8) |
|-----------|--------------------|---------------------|
| Channel I | 43.75 fH = 683 593.75 Hz | 146.25 fH = 2 301 136 Hz |
| Channel II | 68.25 fH = 1 066 406.25 Hz | 178.75 fH = 2 812 499 Hz |
| Deviation | ±100 kHz at 100 %, peak < ±150 kHz (§8.1) | same |
| Subcarrier level | −26 ± 1 dB re unmodulated main carrier (§7.2.2) | same |

- Subcarrier frequencies are derived from fH (line-locked), not rounded
  constants.

### 3.4 EFM path

```
.efm t-values → NRZ channel bits at 4.3218 Mbit/s → low-pass + high-pass + pre-emphasis (Amendment 2 §13.1) → PWM in S6
```

- Filters per IEC 60856/60857 Amendment 2 §13.1.2 (low-pass −3 dB at
  1.75 MHz, < −50 dB above 2.3 MHz, with specified group-delay
  pre-distortion) and Figure 24 (high-pass, pre-emphasis).
- The EFM channel occupies the spectrum below ~1.75 MHz, which in PAL overlaps
  the analogue audio subcarriers; a PAL disc carries either analogue or digital
  audio on that band. The PAL rule must be confirmed against Amendment 2 before
  implementation — **open question**.
- Channel-bit clock relationship to video: the EFM stream is not line-locked;
  the frame index in `.efm.meta` aligns it to video frames.

### 3.5 Pulse-width combination

Audio and EFM shift the zero crossings of the limited FM carrier
(symmetrical double-edge PWM, §7.2.1) rather than being summed linearly into
the RF. S6 forms a slice level *s*(t) from them, and S7 places pit edges where
`cos φ(t) = s(t)`. With video only, *s* ≡ 0.

---

## 4. Module layout

Implemented modules are marked ✔; the rest are planned.

| Module | Directory | Responsibility |
|--------|-----------|----------------|
| ✔ cli | `src/cli/`, `include/ld_creator/cli/` | Command-line parsing and usage text |
| ✔ project | `src/project/` | Project YAML loading, validation, ideal defaults, resolved-project writing |
| ✔ standards | `include/ld_creator/standards/` | Exact NTSC timing constants (header only) |
| ✔ cvbs_input | `src/cvbs_input/` | S1: read `.cvbs` and `.meta`, convert to IRE |
| audio_input | `src/audio_input/` | Read WAV channel pairs |
| efm_input | `src/efm_input/` | Read `.efm` t-values and `.efm.meta` frame index |
| dsp | `src/dsp/` | Shared filters and resamplers |
| video_rate_converter | `src/video_rate_converter/` | S2 |
| group_delay_predistorter | `src/group_delay_predistorter/` | S3 |
| video_pre_emphasis | `src/video_pre_emphasis/` | S4 |
| fm_phase_modulator | `src/fm_phase_modulator/` | S5 |
| recording_signal_combiner | `src/recording_signal_combiner/` | S6 (audio and EFM sources later) |
| recording_limiter | `src/recording_limiter/` | S7 |
| disc_cutter | `src/disc_cutter/` | S8 |
| disc_replicator | `src/disc_replicator/` | S9 |
| playback_mechanics | `src/playback_mechanics/` | S10 |
| optical_readout | `src/optical_readout/` | S11 (✔ readout transfer function) |
| player_front_end | `src/player_front_end/` | S12 |
| rf_capture | `src/rf_capture/` | S13 |
| ✔ rf_sample_writer | `src/rf_sample_writer/` | S14 |
| ✔ diagnostics | `src/diagnostics/` | Byte sinks and diagnostic tap writers |
| ✔ pipeline | `src/pipeline/`, `include/ld_creator/pipeline/` | D1–D10 types, stage interfaces, block chain and threading (✔); run setup against the input (✔); end-to-end wiring (planned) |

All modules are built into the `ld_creator_core` library; `src/main.cpp` is
the only source outside it. Modules depend on interfaces (`I`-prefixed) and
receive dependencies by constructor injection (TESTING.md).

---

## 5. Round-trip harness — *designed*

Functional tests and `scripts/` drive the loop in §1 with the pinned tools:

1. Run the pinned `videosynth` on a committed project YAML into the build tree.
2. Run `ld-creator` on a committed ld-creator project that names that output.
3. Run the pinned `ld-decode` on the RF.
4. Compare decoded output against the truth as defined in
   [../user/ntsc-round-trip-metrics.md](../user/ntsc-round-trip-metrics.md).

Each disc impairment is tested by an A/B pair of ld-creator projects that
differ in one stage block.

---

## 6. Open questions

- PAL analogue/digital audio coexistence on the low band (§3.4).
- Whether ld-creator should accept videosynth's `RAW_S16_*` high-rate presets
  directly, avoiding a 4fsc → modelling-rate resample.
- The linear readout model reaches only the lower part of the IEC 60857
  Figure 18 HFMI band; the phase-pit model is planned to replace it
  ([S11 note](../user/disc-model/s11-optical-readout.md)).
- The RF keeps the readout MTF, as at a player's test point, and ld-decode
  compensates it. At the inner CAV radius, the round-trip chroma and
  frequency-response rows therefore test ld-decode's MTF model against S11's
  linear model, which is itself an approximation (plan Task 10.1). The S12
  diagnostic equaliser bisects a disagreement
  ([metrics](../user/ntsc-round-trip-metrics.md)).
