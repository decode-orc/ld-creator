# Disc model — stage notes

Developer notes for the physical disc model that ld-creator implements. The
work is planned in
[../ntsc-video-modulation-plan.md](../ntsc-video-modulation-plan.md), and the
architecture is in
[../../design/high-level-design.md](../../design/high-level-design.md) §3.

Each pipeline stage has one note. A note states what the stage models, where
each equation and value comes from, and what the stage's implementation must
satisfy. Nothing in a note is *implemented* until the HLD marks that stage
implemented.

| Document | Contents |
|----------|----------|
| [s01-cvbs-input.md](s01-cvbs-input.md) … [s14-rf-sample-writer.md](s14-rf-sample-writer.md) | One note per stage |
| [parameter-register.md](parameter-register.md) | Every parameter, its value, tolerance, source, status and project key |
| [project-file-schema.md](project-file-schema.md) | How the project YAML relates to the model; keys reserved for later phases. The key-by-key reference is [docs/reference/project-file.md](../../../docs/reference/project-file.md) |
| [examples/](examples/) | Example projects: the ideal CAV disc, and a project with every key set |
| [rf-occupancy.md](rf-occupancy.md) | Instantaneous-frequency range, occupied bandwidth and residual above the capture Nyquist frequency for each test pattern (plan Task 6.3) |
| [secondary-sources.md](secondary-sources.md) | Sources beyond the IEC standard, what each establishes, and what is still unsourced |
| [source-acquisition.md](source-acquisition.md) | Sources still to acquire, in order of importance, with links |
| [../ntsc-round-trip-metrics.md](../ntsc-round-trip-metrics.md) | How a round trip is aligned, measured and judged |

## Stages

| # | Note | Module | Input → output |
|---|------|--------|----------------|
| S1 | [s01-cvbs-input.md](s01-cvbs-input.md) | `cvbs_input` | `.cvbs` + `.meta` → D1 |
| S2 | [s02-video-rate-converter.md](s02-video-rate-converter.md) | `video_rate_converter` | D1 → D2 |
| S3 | [s03-group-delay-predistorter.md](s03-group-delay-predistorter.md) | `group_delay_predistorter` | D2 → D2 |
| S4 | [s04-video-pre-emphasis.md](s04-video-pre-emphasis.md) | `video_pre_emphasis` | D2 → D2 |
| S5 | [s05-fm-phase-modulator.md](s05-fm-phase-modulator.md) | `fm_phase_modulator` | D2 → D3 |
| S6 | [s06-recording-signal-combiner.md](s06-recording-signal-combiner.md) | `recording_signal_combiner` | D3 → D3 |
| S7 | [s07-recording-limiter.md](s07-recording-limiter.md) | `recording_limiter` | D3 → D4 |
| S8 | [s08-disc-cutter.md](s08-disc-cutter.md) | `disc_cutter` | D4 → D5 |
| S9 | [s09-disc-replicator.md](s09-disc-replicator.md) | `disc_replicator` | D5 → D6 |
| S10 | [s10-playback-mechanics.md](s10-playback-mechanics.md) | `playback_mechanics` | D6 → D7 |
| S11 | [s11-optical-readout.md](s11-optical-readout.md) | `optical_readout` | D7 → D8 |
| S12 | [s12-player-front-end.md](s12-player-front-end.md) | `player_front_end` | D8 → D9 |
| S13 | [s13-rf-capture.md](s13-rf-capture.md) | `rf_capture` | D9 → D10 |
| S14 | [s14-rf-sample-writer.md](s14-rf-sample-writer.md) | `rf_sample_writer` | D10 → `.s16` + resolved project |

## Data representations

Two time axes run through the model:

- **Recording time `t`** (S1–S8): the time at which the mastering chain
  processes the signal. `t = 0` is 0H of frame line 1 of the first input
  frame.
- **Playback time `τ`** (S10–S14): the time at which the player reads the
  disc. On the ideal disc the player is locked to the recorded syncs, so
  `τ = t` (S10).

| D | Produced by | Contents | Units | Representation |
|---|-------------|----------|-------|----------------|
| D1 | S1 | Composite video | IRE | `double` samples at 4fsc. Sample *n* is at `t = (n + 57/90) / 4fsc` (S1) |
| D2 | S2–S4 | Composite video | IRE | `double` samples at the modelling rate `Fm`, same time origin |
| D3 | S5–S6 | Carrier phase φ, its frequency f = φ′/2π, and slice level *s* | rad; Hz; dimensionless | Three `double` sequences at `Fm`, one sample more than D2 (the end of the run). *s* ≡ 0 for video only (S6) |
| D4 | S7 | Recorded transitions | s | Ordered list of (`t_k`, polarity), `double`. Polarity alternates pit-start/pit-end |
| D5 | S8 | Cut pit map | m | Ordered list of pit edges as (arc length `x_k` along the spiral, radius `r_k`, polarity), plus the cutting trajectory `x(t)`, `r(t)` |
| D6 | S9 | Pressed pit map | m | D5 after replication. Edge geometry attributes (width, depth, slope) are added in Phase 8 |
| D7 | S10 | Scan trajectory and pits | m, s | D6 plus the readout spot's track position `x(τ)` and radial offset `ρ(τ)` |
| D8 | S11 | Detector photocurrent | normalised: unpitted land = 1 | `double` samples at the readout rate `Fr` = 80 MHz (S11) |
| D9 | S12 | Player RF output (test point, unequalised by default) | normalised, as D8 (land = 1) | `double` samples at `Fr` |
| D10 | S13 | Captured RF | ADC codes | Integer codes at 40 MHz (10-bit for the DomesdayDuplicator profile) |

Precision of the edge lists: a `double` resolves 4.5 × 10⁻¹³ s at
`t` = 3600 s (a full CLV side), which is well inside the 4 ps edge-time
budget of S7. Arc length on a full CLV side (≈ 41 km) is resolved to about
10⁻¹¹ m.

## Stage-note template

Every note uses these headings, in this order. A heading with nothing to say
says so; unknowns are listed under **Open items**, never left blank.

1. **Physical process** — what happens on the real disc, in plain words.
2. **Sources** — the documents, sections and figures the note relies on.
3. **Model and equations** — every equation cited or derived.
4. **Parameters** — a table that points to the
   [parameter register](parameter-register.md) rows.
5. **Assumptions and approximations** — each with the reason and an error
   bound.
6. **Input/output contract** — the D-representation in and out, units and
   time origin.
7. **Project YAML block** — keys, types, ranges and defaults. The
   authoritative definition is [docs/reference/project-file.md](../../../docs/reference/project-file.md).
8. **Ideal setting** — what the stage does when its block is omitted.
9. **Verification** — how the implementation is checked against this note.
10. **Diagnostic tap** — what the tap writes.
11. **Open items** — anything unsourced or undecided.

## Decisions

Decisions taken in Phase 1 (plan Task 1.6). Each is recorded in full in the
note named.

| Question | Decision | Recorded in |
|----------|----------|-------------|
| Is the §9.2.3 deviation measured before or after pre-emphasis? | The question does not arise: the §9.2.4 pre-emphasis has unity DC gain, so the static levels that define the deviation are the same before and after it | [S4](s04-video-pre-emphasis.md), [S5](s05-fm-phase-modulator.md) |
| Is pre-emphasis overshoot clipped before the limiter? | No. §9.2.4 is a linear transfer function and no source documents a clipper. A clipper is added only if a source documents one | [S4](s04-video-pre-emphasis.md) |
| Is group-delay pre-distortion on by default? | Yes. §9.1.7 says it "shall" be applied. Disabling it is an out-of-tolerance setting | [S3](s03-group-delay-predistorter.md) |
| What is the reference capture front end? | The DomesdayDuplicator, modelled from its schematics and test data. An `ideal` profile stays available for bisecting faults | [S13](s13-rf-capture.md) |
| How does a sweep go outside an IEC tolerance? | A top-level `allow_out_of_tolerance` list of key paths | [project-file-schema.md](project-file-schema.md) |
| Where do physical dropouts belong? | In ld-creator, as an RF-domain disc defect (later impairment phase), so that ld-decode's RF dropout detection can be measured. videosynth's CVBS dropouts are not used in round-trip inputs | HLD §1.1, [S9](s09-disc-replicator.md) |
| Does the disc have a default format, radius and track pitch? | No. IEC 60857 gives ranges, not nominal values, so `disc_cutter` is a required block. The example projects document their choices | [S8](s08-disc-cutter.md) |
