# NTSC video on a modelled disc — implementation plan

Turn an NTSC CVBS recording from videosynth into the RF that a capture device
would record from a real LaserVision disc. Then confirm that ld-decode
recovers the original.

The signal path follows a real disc's life, from beginning to end:

1. video signal processing at the mastering facility;
2. the laser beam recorder that cuts the master;
3. replication into pressed discs;
4. the player's mechanics, optics and front end;
5. the capture device.

The plan builds each step as a separate **pipeline stage**. Each stage can be
implemented, documented, tested and inspected on its own.

Scope: NTSC video only. Analogue audio, EFM and PAL slot into stages that
already exist (marked *later* below), so they need no restructuring.

---

## Ground rules

These rules apply to every phase.

### No unexplained formulas

- Every equation, constant and approximation in a stage traces to one of
  three things:
  - a **cited source** (document, section, figure);
  - a **derivation** from cited quantities, written out in the stage note;
  - a **declared modelling decision**, with its reason and an error bound.
- A value with no source yet is registered as **unsourced**. It is set to
  the stage's *ideal* (identity) behaviour and listed as an open item. It is
  never guessed or tuned.
- Fitting a parameter to **measured data** is allowed only when the dataset
  is named (e.g. BBC RD 1986/01 Fig. 6) and the fitting method is documented.
- Code constants carry AGENTS.md §4.3.6 specification comments. Each also
  appears in the parameter register (Task 1.3). A constant with no register
  entry fails review.

### The project file is the complete description of a run

Like videosynth, an ld-creator run is described by a **YAML project file**.
It holds every input, every stage parameter, every random seed and every
output.

- **Input:** the project file and the media it names.
- **Output:** byte-identical RF for the same project file and media,
  whatever the thread count, block size or host.
- **Nothing hidden:** nothing that affects the output may come from the
  command line, the environment, the clock or a default that isn't
  documented.
- **What stays on the command line:** the options that cannot change the
  output, matching videosynth's command line:
  - `--project`;
  - `--validate`;
  - `--output-root`;
  - `--threads`;
  - `--log-level`;
  - `--log-file`.
- **One block per stage.** Each pipeline stage has its own YAML block. A
  block that is left out, or a key that is left out, takes the documented
  **ideal** value from the parameter register. A project with only `input`
  and `output` therefore describes the ideal disc.
- **Every run is recorded.** Each run writes a **resolved project** next to
  the RF. It contains every key with its effective value, plus the
  ld-creator version and hashes of the input media. Running that resolved
  file reproduces the RF byte for byte. It replaces the separate "sidecar"
  that the design previously called for.
- **Strict parsing.** Unknown keys, values outside their range, and values
  outside their IEC tolerance (unless explicitly allowed for a sweep) are
  validation errors, not silent fixes.

### ld-decode is the instrument under test

- Do not read ld-decode's demodulator, filters, constants or defaults to
  choose any stage parameter. If ld-creator copies ld-decode's assumptions,
  its errors cancel out and the harness measures nothing.
- Only ld-decode's **external interface** may be used: its command line, the
  RF formats and rate it reads (`.s16`/`.lds`/`.ldf`, 40 MHz default, `-f`),
  and its CVBS output.
- A round-trip discrepancy is never fixed by moving a stage towards ld-decode
  without a source-backed reason (TESTING.md, "Round-trip verification").

### Sources

**Primary (normative):** in
[docs-tech/analogue-video-specifications/docs/laserdisc/](../analogue-video-specifications/docs/laserdisc/):
- [IEC 60857-1986](../analogue-video-specifications/docs/laserdisc/IEC-60857-1986-Laservision-NTSC/IEC-60857-1986-Laservision-NTSC.md)
  and its amendments
  [1](../analogue-video-specifications/docs/laserdisc/IEC-60857-1986-Laservision-NTSC-Amendment-1/IEC-60857-1986-Laservision-NTSC-Amendment-1.md)
  and
  [2](../analogue-video-specifications/docs/laserdisc/IEC-60857-1986-Laservision-NTSC-Amendment-2/IEC-60857-1986-Laservision-NTSC-Amendment-2.md).
  Sections used:
  - §4 disc mechanics;
  - §9 video;
  - §11 playback measurement system;
  - §12 operational signals;
  - Figures 8, 9, 17, 18, 19.

**Video levels and timing:**
- [SMPTE 170M-2004](../analogue-video-specifications/docs/video_formats/SMPTE-170M-2004/SMPTE-170M-2004.md)
- [SMPTE 244M-2003](../analogue-video-specifications/docs/video_formats/SMPTE-244M-2003/SMPTE-244M-2003.md)
- The CVBS spec's
  [NTSC preset](../cvbs-file-format-specification/docs/video-standard-presets.md)
  and
  [sample encodings](../cvbs-file-format-specification/docs/sample-encoding-presets.md).

**Measured data:**
[BBC RD 1986/01](../analogue-video-specifications/docs/laserdisc/BBC-Experiments_with_digitally_mastered_laservision_optical_discs-1986-01/BBC-Experiments_with_digitally_mastered_laservision_optical_discs-1986-01.md):
- how pit spacing and duty cycle carry the signal;
- cutting-spot size;
- `fc = 2·NA/λ`;
- photodiode/preamp response (Fig. 8);
- replay spectra and blank-disc noise (Fig. 9);
- attenuation against radius (Fig. 6).

**Secondary (to acquire in Task 1.4):**
- Philips Technical Review vol. 33 (1973), the VLP issue. Covers system,
  signal processing, optical readout and mastering.
- Bouwhuis et al., *Principles of Optical Disc Systems* (1985). Covers the
  readout optics, MTF, phase-pit signal formation and crosstalk.
- Isailović, *Videodisc and Optical Memory Systems* (1985). Covers mastering
  and replication.
- F. Panter, *Modulation, Noise and Spectral Analysis*, p. 542, which
  IEC 60857 §7.2.1 cites for pulse-width modulation.
- DomesdayDuplicator hardware documentation, for the capture front end.

If the sources are silent or ambiguous on a choice that changes what
ld-decode is measured against, ask the user (AGENTS.md §12).

---

## Pipeline

Each stage consumes one documented data representation and produces the
next. Each stage's parameters live in its own block of the project YAML.
Every representation can be written out as a **diagnostic tap** for
inspection. Taps are declared in the project's `diagnostics:` block and do
not change the RF.

| # | Stage (module) | Physical process | Input → output | Initial model and source | Ideal (identity) setting |
|---|----------------|------------------|----------------|--------------------------|--------------------------|
| S1 | `cvbs_input` | Programme source | `.cvbs`/`.meta` → **D1** video in IRE at 4fsc | SMPTE 244M levels from `.meta` | — |
| S2 | `video_rate_converter` | Mastering electronics run at the modelling rate | D1 → **D2** video in IRE at the modelling rate `Fm` | Rate and resampler error bound documented | — |
| S3 | `group_delay_predistorter` | Video pre-distortion before cutting | D2 → D2 | IEC 60857 §9.1.7, Figure 8 | Bypass (A/B only) |
| S4 | `video_pre_emphasis` | Video pre-emphasis | D2 → D2 | §9.2.4: `(1+s·320 ns)/(1+s·120 ns)` | — |
| S5 | `fm_phase_modulator` | FM modulator | D2 → **D3** carrier phase φ(t) | §9.2.1–9.2.3: 8.1 MHz at blanking, 1.7 MHz sync→white | — |
| S6 | `recording_signal_combiner` | Adds audio/EFM pulse-width modulation (*later*) | D3 → D3 | §7.2.1; Amendment 2 §13.1.5 | Pass-through (video only) |
| S7 | `recording_limiter` | Recorder limiter: the cut is binary | D3 → **D4** transition times | Edges at φ = (k + ½)·π (BBC RD 1986/01 §1.1) | — |
| S8 | `disc_cutter` | Laser beam recorder: time → track position, cutting-spot exposure | D4 → **D5** cut pit map (track coordinate, edge profile) | §4.7 CAV/CLV velocity; cutting spot ≈ 0.6 µm (BBC §4) | Perfect edges, no length bias |
| S9 | `disc_replicator` | Mastering → stamper → pressed disc | D5 → **D6** pressed pit map | Unsourced until Task 1.4; bounded by §12.3 HFMI and §12.1 RMI/RRI | Exact copy |
| S10 | `playback_mechanics` | Spindle and track motion: playback time → track coordinate | D6 → **D7** scan trajectory and pits | §4.7 speed, §4.18 eccentricity, §4.16 vertical deviation | Perfect concentric track at nominal speed |
| S11 | `optical_readout` | Spot scans the pits; detector photocurrent | D7 → **D8** photocurrent i(t) | §11.1 (λ 632.8 nm, NA 0.40, 23 % rim); linear MTF first, phase-pit model later (Bouwhuis) | IEC §11 reference optics; adjacent tracks blank |
| S12 | `player_front_end` | Photodiode, preamplifier, player RF path | D8 → **D9** player RF | §11.2 "adequate bandwidth"; BBC Fig. 8 for a real player | IEC §11.2 flat response, noiseless |
| S13 | `rf_capture` | Capture device: anti-alias filter, sampling, quantisation | D9 → **D10** RF samples | DomesdayDuplicator documentation (unsourced until obtained) | Band-limit at Nyquist, 16-bit |
| S14 | `rf_sample_writer` | File output and resolved project | D10 → `.s16` (+ resolved project YAML) | ld-decode input interface | — |

**Pit-edge representation.** D4–D7 carry pit **edges** as event lists in
double precision, not sampled waveforms. The physical disc is a sequence of
pit edges. This representation is exact, has no aliasing before S11, and
lets each geometry stage act on individual edges. S11 turns the edges back
into a waveform: for a linear readout, the signal is a sum of the spot's
**edge response** at each edge. That response follows from the optics, and
its derivation lives in S11's stage note.

**Ideal disc.** With every stage at its ideal setting, the pipeline describes
a disc that is nominal in every IEC parameter, read by the IEC §11 reference
system. This baseline still includes the reading spot's readout loss,
because no disc can be read without it. Each impairment phase then adds one
physical effect, which is measured with an A/B comparison against this
baseline.

**Carrier diagnostic.** A project can select `output.mode: carrier_diagnostic`
to render `cos φ` directly at the output rate. It is explicitly **not a disc**. It exists only
to bisect faults: if the carrier decodes but the disc does not, the fault is
in S7 onward.

---

## Phase 1 — Architecture, stage notes and sources

Documentation only.

### Task 1.1 — Widen the design scope
- Update [high-level-design.md](../design/high-level-design.md):
  - §1.1: ld-creator now models the physical disc chain. Disc-surface
    impairments that act in the RF domain belong here. CVBS-domain
    impairments stay in videosynth. Decide where physical dropouts belong,
    and record the decision.
  - §3: replace the signal model with the pipeline above.
  - §4: list the stage modules.
  - §2: the project YAML becomes the input interface, and the resolved
    project replaces the sidecar. Close the sidecar question in §6.
- **Acceptance:** the HLD and this plan agree. Every stage is marked
  *designed*.

### Task 1.2 — Stage-note template and notes
- Create `docs-tech/user/disc-model/`, with one note per stage
  (`s01-cvbs-input.md` … `s14-rf-sample-writer.md`). Each note uses fixed
  headings:
  - Physical process;
  - Sources;
  - Model and equations, each cited or derived;
  - Parameters;
  - Assumptions and approximations, with an error bound each;
  - Input/output contract (D-representation, units, timing origin);
  - Project YAML block (keys, types, ranges, defaults);
  - Ideal setting;
  - Verification;
  - Diagnostic tap.
- **Acceptance:** every stage has a note. Unknowns are listed as open items,
  not left blank.

### Task 1.3 — Parameter register
- Create `docs-tech/user/disc-model/parameter-register.md`. It has one row
  per parameter, with these columns:
  - stage;
  - name;
  - value;
  - tolerance;
  - unit;
  - source or derivation;
  - status (`sourced` / `derived` / `calibrated` / `unsourced-ideal`);
  - project YAML key, or `fixed` for a constant the standard defines exactly
    (e.g. fH).
- Seed it from IEC 60857:
  - §9.2.2–9.2.4;
  - §9.1.7;
  - §4.7, §4.11–4.12, §4.15–4.18;
  - §11.1;
  - §12.2.2, §12.3.
- **Acceptance:**
  - Every number in the pipeline table appears in the register.
  - Every YAML key in the project schema (Task 1.7) has exactly one register
    row.

### Task 1.4 — Acquire secondary sources
- Obtain the secondary sources. For each one, record what it establishes for
  each stage it covers, and which `unsourced` rows it resolves.
- Propose material for the upstream `analogue-video-specifications`
  repository. Never edit the submodule here.
- **Acceptance:** the register's `unsourced` list is updated, and each
  remaining entry has a named candidate source or measurement.

### Task 1.5 — Round-trip predictions and metrics
- Create `docs-tech/user/ntsc-round-trip-metrics.md`, defining:
  - how decoded output is aligned to the truth (VBI picture number, then 0H
    per the CVBS stored-origin rule);
  - how many leading frames to discard while ld-decode settles;
  - the measurements on each VITS line;
  - the predicted values for the ideal disc, with pass thresholds:
    - levels within ±1 IRE;
    - VBI codes 100 % correct;
    - chroma amplitude ±2 % and phase ±1°;
    - multiburst and 2T response.
- **Acceptance:** the Phase 7 comparator can be written from this document
  alone.

### Task 1.6 — Questions for the user
At minimum:
- Is IEC deviation measured before or after pre-emphasis?
- Is pre-emphasis overshoot clipped before the limiter?
- Is group-delay pre-distortion on by default?
- What is the capture device's reference front end?
- How should IEC tolerances be overridden in sweep projects? For example, an
  explicit `allow_out_of_tolerance` flag on each key.

**Acceptance:** answers are recorded in the relevant stage notes.

### Task 1.7 — Project file schema
- Design the project YAML, following videosynth's conventions:
  - `project:` identity;
  - `{project}` / `{output}` path roots;
  - quoted versions;
  - strict keys.
- Top-level blocks:
  - `input` (the videosynth `.cvbs`, from which the `.meta` and media
    follow);
  - one block per stage S2–S13;
  - `output` (path, format, mode);
  - `diagnostics` (taps).
- Write an example for the ideal disc, and one with every key set.
- **Acceptance:**
  - The schema is documented in a design note, in the same per-key table
    style as videosynth's reference pages.
  - The schema matches the register one-to-one.
  - No output-affecting setting exists outside the schema.

---

## Phase 2 — Pipeline framework and input (S1, S14 skeleton)

### Task 2.1 — Stage interfaces
- Typed C++ representations D1–D10 and an `I`-prefixed interface per stage,
  with constructor injection and block-based streaming that respects field
  and line boundaries.
- **Acceptance:**
  - Unit tests chain mock stages, and output is identical across block
    sizes and thread counts.
  - Each D-type's units and time origin are documented in its header.

### Task 2.2 — Project loader and validator
- Add yaml-cpp to the flake and CMake, as in videosynth. Then build:
  - a parser for the Task 1.7 schema into typed per-stage configuration
    structs;
  - an interface over the file so unit tests can parse YAML text in memory;
  - strict validation (unknown keys, ranges, IEC tolerances);
  - ideal defaults taken from the register.
- **Acceptance:**
  - Unit tests cover each block, its defaults and its errors.
  - An error names the offending key path, in the style of videosynth's
    troubleshooting messages.

### Task 2.3 — Resolved project and diagnostic taps
- Serialise the effective configuration as a resolved project:
  - every key, with keys in a fixed order;
  - the ld-creator version;
  - SHA-256 hashes of the input media.
- Write the taps declared in `diagnostics:`:
  - sampled data as raw arrays plus a descriptor;
  - edge lists as binary records plus a descriptor.
- **Acceptance:**
  - Loading a resolved project and resolving it again gives byte-identical
    YAML.
  - Taps use an injected sink in unit tests.
  - Formats are documented in `docs/`.

### Task 2.4 — CVBS input (S1)
- Metadata reader (SQLite) and sample reader for `CVBS_S16_4FSC`,
  `CVBS_U10_4FSC` and `CVBS_TPG21_4FSC`. Converts to IRE using the preset
  levels and `.meta` overrides. Rejects input that isn't NTSC.
- **Acceptance:**
  - For each encoding, unit tests map sync, blanking, black and white to
    −40, 0, 7.5 and 100 IRE (to 1e-9).
  - A truncated frame is reported.

### Task 2.5 — Command line
- Add these options, following videosynth's command line:
  - `--project <path>` (required except with `--version`);
  - `--validate`;
  - `--output-root`;
  - `--threads`.

  No option may change the output.
- **Acceptance:**
  - CLI unit tests pass.
  - [cli-options.md](../../docs/reference/cli-options.md) and a new
    project-file reference in `docs/reference/` are written.
  - A functional test validates the example projects.

---

## Phase 3 — Video signal stages (S2–S5)

Each filter is checked in unit tests against its analytic source curve, not
against stored outputs.

### Task 3.1 — Rate converter (S2)
- Choose the modelling rate `Fm` in S2's note. The reason must come from
  S5 and S7: the error bound on edge times interpolated from φ, against the
  §9.2.3 deviation tolerance.
- **Acceptance:** the passband error and group delay predicted in the note
  are met in unit tests at 0.5, 3.58 and 4.2 MHz.

### Task 3.2 — Group-delay pre-distortion (S3)
- Match the §9.1.7 curve relative to 0.5 MHz, after removing a documented
  bulk delay. Magnitude stays flat.
- **Acceptance:** group delay at the six table frequencies is within the
  middle third of each tolerance; magnitude is ±0.1 dB to 4.2 MHz.

### Task 3.3 — Pre-emphasis (S4)
- Discretise the §9.2.4 shelf, with the method and warping error documented.
- **Acceptance:** within 0.05 dB of `|H(jω)|` from 0.1 to 6 MHz, with DC
  gain of 1.

### Task 3.4 — FM phase modulator (S5)
- φ(t) in double precision. Instantaneous frequency is
  `8.1 MHz + IRE × (1.7 MHz / 140)`, which follows from §9.2.2 and §9.2.3.
  The derivation is in S5's note.
- **Acceptance:**
  - Constant −40, 0 and 100 IRE inputs give 7.614286, 8.1 and 9.314286 MHz
    (from the slope of φ, to < 1 Hz).
  - Output is deterministic.

### Task 3.5 — Carrier diagnostic
- `output.mode: carrier_diagnostic` renders `cos φ` at 40 MHz through the
  S13/S14 writer path. The resolved project labels it not-a-disc.
- **Acceptance:** the functional test output has the expected length, and
  the resolved project flags it as a diagnostic.

---

## Phase 4 — Mastering stages (S6–S8)

### Task 4.1 — Signal combiner (S6)
- An interface with a pass-through implementation. Audio and EFM implement
  it later.
- **Acceptance:** identity unit test.

### Task 4.2 — Recording limiter (S7)
- Finds each crossing φ = (k + ½)·π by interpolating φ inside the sample,
  using the method and error bound from the note. Emits alternating pit and
  land edges.
- **Acceptance:**
  - On a constant-frequency input, edge intervals equal `1/(2f)` to the
    documented bound.
  - Edges alternate polarity.

### Task 4.3 — Disc cutter (S8)
- Maps time to track coordinate.
  - CAV: one revolution per frame (§4.7.1), at a configured start radius
    within §4.10–4.12.
  - CLV: constant linear velocity within §4.7.2, with track pitch from
    §4.15.
- Edge profile hooks are written now; at the ideal setting there is no
  length bias and edges are perfect.
- **Acceptance:** unit tests check radius after N revolutions against the
  pitch, and pit spatial frequency against Figure 18's note:
  `V = f / (2πR·fr)`.

---

## Phase 5 — Ideal replication, playback and capture (S9–S14)

The first complete RF from the ideal disc.

### Task 5.1 — Ideal replicator and mechanics (S9, S10)
- An exact copy, and a perfect concentric track at nominal speed. Both
  follow the interfaces that Phase 8 uses for impairments.
- **Acceptance:** identity unit tests.

### Task 5.2 — Optical readout, linear model (S11)
Derive the following in S11's note, from §11.1 and Bouwhuis:
- the line-scan **edge response**;
- the transfer function, with cut-off `2·NA/λ`;
- the effect of the 23 % rim intensity.

Rendering: `i(t) = i₀ + Σₖ ±Δ·ESF(x(t) − xₖ)`, evaluated at the output
sample instants. The table resolution comes from a stated error bound.

**Acceptance:**
- Unit tests match the analytic transfer function.
- A constant-frequency pit train gives the modulation depth the model
  predicts.
- Predicted HFMI falls inside the §12.3 / Figure 18 limits; if it does not,
  the shortfall is recorded as a finding about the linear model.

### Task 5.3 — Ideal front end and capture (S12, S13)
- The ideal front end follows §11.2 ("adequate bandwidth", flat).
- Capture applies a documented anti-alias band-limit below 20 MHz before
  sampling at 40 MHz. The outer-radius CAV optical cut-off (~34.5 MHz)
  exceeds Nyquist, so this is required, not optional.
- Quantise to 16 bits with documented headroom.
- **Acceptance:** above Nyquist, the spectrum is down by the documented
  amount. No clipping on any test pattern.

### Task 5.4 — End-to-end pipeline
- Wire S1–S14 together. Disc type and start radius come from the S8
  `disc_cutter` block.
- **Acceptance:**
  - A functional test runs a committed ld-creator project on a small
    committed videosynth project, and produces RF of the expected length.
  - Output is byte-identical across thread counts.
  - Re-running from the resolved project gives the same bytes.
  - User docs are updated.

---

## Phase 6 — Independent verification

Check the RF against the stage notes **without ld-decode**. The analysis
scripts live in `scripts/` (numpy, dev shell) and are written from first
principles.

### Task 6.1 — Analysis tools
- An instantaneous-frequency estimator, edge-time extraction from taps,
  spectra, and a tap reader.
- **Acceptance:** recovers a synthetic tone to < 1 kHz and synthetic edges to
  < 0.1 ns.

### Task 6.2 — Stage-by-stage checks
Check each tap against its note:
- S5: carrier at sync, blanking and white within ±5 kHz;
- S3/S4: response from multiburst and 2T, within Phase 3 tolerances;
- S7: edge timing;
- S11: modulation depth against radius.

**Acceptance:** a results table in each stage note.

### Task 6.3 — Spectrum and occupancy
For each test pattern, record:
- the peak and minimum instantaneous frequency (pre-emphasis overshoot);
- the RF occupied bandwidth;
- the residual above Nyquist.

**Acceptance:** results recorded. Anything that contradicts a stage note is
raised as an open item.

---

## Phase 7 — Round trip on the ideal disc

### Task 7.1 — videosynth project
- Commit a 100-frame NTSC CAV project to `tests/functional/`, with:
  - `CVBS_S16_4FSC` encoding (the VBI burst dips below sync tip);
  - `laserdisc.vbi_burst: true`;
  - `placement: laserdisc` VITS: `virs` on 19/282, `ntc7-composite` on 20,
    `ntc7-combination` on 283;
  - `picture_number` from 1, plus `fm_picture_number` and `fm_white_flag`.
- Content: 75 % bars, a luma ramp, multiburst, and flat grey.
- No `laserdisc.channel` impairments in videosynth: the disc effects come
  only from ld-creator.
- Alongside it, commit one ld-creator project per baseline configuration in
  Task 7.4. Each names the videosynth output through `{output}` and differs
  from the others only in its stage blocks.
- **Acceptance:** `videosynth --validate` and `ld-creator --validate` pass
  on all of them.

### Task 7.2 — Runner and comparator
- A script that runs these steps into the build tree only:
  1. videosynth;
  2. ld-creator;
  3. `ld-decode --NTSC`;
  4. the comparator from Task 1.5.

  Tool versions are logged.
- **Acceptance:**
  - Comparing the truth against itself passes.
  - A copy offset by +2 IRE fails.
  - The run reproduces from a clean build.

### Task 7.3 — Functional test
- A GoogleTest wrapper that uses `GTEST_SKIP()` when either tool is missing
  from `PATH`, labelled `functional`.
- **Acceptance:** not in the unit lane.

### Task 7.4 — Baseline results
Run the round trip on:
- the carrier diagnostic;
- the ideal disc, CAV inner radius (55 mm);
- the ideal disc, CAV outer radius (145 mm);
- the ideal disc, CLV.

**Acceptance:** each prediction is marked confirmed or refuted, with
numbers, in the metrics document.

### Task 7.5 — Triage
For each refuted prediction:
- bisect with the carrier diagnostic and the taps;
- check the stage against its note;
- if the stage conforms, record an ld-decode finding with evidence in
  `docs-tech/user/ld-decode-findings.md`;
- otherwise fix the stage and its note, then rerun.

**Acceptance:** every discrepancy is classified with evidence.

---

## Phase 8 — Deterministic disc impairments

Each task adds one physical effect, with this sequence:
1. stage note;
2. register entries;
3. implementation, which stays **off** until enabled in the stage's project
   block;
4. unit tests against the note;
5. A/B round trip against the Phase 7 baseline. The two projects differ only
   in that one block, and the variant is committed alongside the baseline.

Parameters come only from sourced or calibrated register rows.

### Task 8.1 — Eccentricity and speed variation (S10)
- Up to the §4.18 radial-deviation limit. The resulting track-speed and
  timing modulation is derived in the note.
- **Acceptance:** the carrier modulation at the rotation rate measured from
  the tap matches the derivation.

### Task 8.2 — Cutting exposure and pit length bias (S8)
- Model the cutting-spot profile and resist threshold from the Task 1.4
  sources. Pit and land length asymmetry follows from the model, not from a
  free parameter.
- **Acceptance:** tap edge shifts match the model, and the 2nd-harmonic
  level matches its prediction.

### Task 8.3 — Replication geometry (S9)
- Pit depth, width and wall slope, and how faithfully replication copies
  them, from the Task 1.4 sources.
- **Acceptance:** S11's predicted HFMI with this geometry is inside the
  Figure 18 limits.

### Task 8.4 — Focus and tilt (S10 → S11)
- The spot changes with vertical deviation (§4.16) and the §11.3
  depth-of-focus and tilt limits.
- **Acceptance:** the change in MTF matches the note's derivation.

### Task 8.5 — Results
- Run A/B round trips for each effect, and for all of them together. Sweep
  each effect to its IEC limit. Every sweep point is a project file
  (generated by the runner into the build tree, with its resolved project
  kept).
- **Acceptance:** results and triage (as in Task 7.5) are recorded. A
  failure on a disc inside the standard is an ld-decode finding.

---

## Phase 9 — Noise and crosstalk

Pseudo-random effects use documented generators seeded from the project
YAML. Each stage has its own seed key, so turning one effect on or off does
not change another effect's random sequence.

### Task 9.1 — Disc and detector noise (S11, S12)
- The noise spectrum comes from BBC Fig. 9f (blank disc) and the front-end
  noise. The calibration method is documented. Video SNR is checked against
  §12.2.2 (≥ 30 dB unweighted at blanking).
- **Acceptance:** SNR measured from the demodulated tap matches the
  prediction.

### Task 9.2 — Adjacent-track crosstalk (S11)
- Neighbouring track pits read by the spot tails, at the §4.15 track pitch
  (Bouwhuis). CAV crosstalk is line-locked; CLV crosstalk is not.
- **Acceptance:** crosstalk level against pitch matches the note.

### Task 9.3 — Real player front end (S12)
- An optional player profile from BBC Fig. 8, kept separate from the IEC
  §11.2 reference.
- **Acceptance:** unit tests match the documented response.

### Task 9.4 — Results
- Run A/B round trips, and sweep noise down to the §12.2.2 minimum.
- **Acceptance:** results and triage recorded.

---

## Phase 10 — Readout model refinement and calibration

### Task 10.1 — Phase-pit readout (S11)
- Replace the linear edge-response model with the scalar-diffraction
  readout of phase pits (Bouwhuis), keeping the linear model selectable.
- **Acceptance:** the two models agree in the small-signal limit. The
  differences that remain (edge shifts between neighbouring pits,
  harmonics) are documented.

### Task 10.2 — Calibration against measured data
- Compare predicted channel attenuation against radius with BBC Fig. 6 and
  Fig. 9.
- **Acceptance:** any difference is explained, or a `calibrated` register
  row is added naming the dataset and the fit.

### Task 10.3 — Tolerance sweep and close-out
- Sweep every IEC-toleranced parameter (§9.1.7, §9.2.2–9.2.4) to both edges
  on the full disc model. Mark the HLD stages *implemented*.
- **Acceptance:** results table recorded, and documentation matches the code
  (AGENTS.md §9).
