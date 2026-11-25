# NTSC round-trip metrics

How a round trip (videosynth → ld-creator → ld-decode) is aligned, measured
and judged. The plan's Phase 7 comparator is written from this document alone
([ntsc-video-modulation-plan.md](ntsc-video-modulation-plan.md), Tasks 7.2
and 7.4).

The comparator reads both files through their CVBS metadata and decodes the
VBI itself, from IEC 60857 §10. It uses nothing from ld-decode except its
output files.

## Hypothesis

For the ideal disc ([disc-model](disc-model/README.md), every stage at its
ideal setting), a decoder that follows IEC 60857 recovers the input:

- every level, frequency response and timing measurement below equals the
  truth within its threshold;
- every VBI code decodes exactly.

Each measurement is reported as a number, and each threshold is marked
**confirmed** or **refuted** in Task 7.4. A refuted prediction is triaged
(Task 7.5); it is never fixed by adjusting the disc model towards the decoder.

## Test material

The videosynth project of plan Task 7.1,
[`tests/functional/projects/ntsc_round_trip_videosynth.yaml`](../../tests/functional/projects/ntsc_round_trip_videosynth.yaml):
100 NTSC CAV frames, `CVBS_S16_4FSC`, with `laserdisc.vbi_burst` and no
`laserdisc.channel` impairments.

| Frames | Content | VBI |
|--------|---------|-----|
| 0–49 | Lead-in: flat field | Lead-in code `88FFFF` on lines 17, 18, 280 and 281 (§10.1.1); white flag on line 11 (§10.2.1) |
| 50–99 | Programme: 75 % colour bars (13 frames), luma ramp (13), multiburst (12), flat field (12) | CAV picture numbers 1–50 on lines 17 and 18 (§10.1.3); FM picture numbers on lines 10 and 273 (§10.2.3); first-field white flag on line 11 (§10.2.4) |

Every frame carries VIRS on lines 19 and 282 (§9.1.3), the NTC-7 composite
signal on line 20, and the NTC-7 combination signal on line 283 (§9.1.4).

Three points about the material as built (declared):

- **Legal range only.** The pictures are legal-range sources. 100 % bars and
  full-range ramps are not used: no programme carries them.
- **Flat field.** The bundled sources have no flat grey field, so the flat
  field is `BLACK.exr` (setup, 7.5 IRE). Its role here, a steady level for
  the decoder to settle on and a baseline for P1, does not depend on the
  level.
- **No FM code in the lead-in.** §10.2.3 puts FM picture number 0 on lines 10
  and 273 during the lead-in. The pinned videosynth does not generate it:
  its validator accepts `fm_picture_number` only in the programme area
  ([videosynth#31](https://github.com/decode-orc/videosynth/issues/31)). V1
  compares the decoded lead-in with the truth, so it checks that the decoder
  invents no code there.

**Lead-in length (declared).** §10.1.1 requires at least 1.5 mm of lead-in,
about 900 tracks at the example pitch. A 900-frame lead-in is out of
proportion to the test, so the project uses 50 lead-in frames. If ld-decode
has not settled by the first programme frame, that is classified as a
lead-in shortfall (below), and the remedy is a longer lead-in in the project,
not discarding frames.

## Alignment

### Frames

1. Decode the CAV picture number of every frame of both files: the 24-bit
   biphase code `FX₁X₂X₃X₄X₅` on lines 17/18 or 280/281 (§10.1.3), and the
   40-bit FM code on lines 10 and 273 (§10.2.3, Figure 13).
2. Pair programme frames by picture number.
3. Pair lead-in frames by their position before the first programme frame
   (lead-in frames have no number to pair them by).
4. Report any decoded frame without a valid picture number, any repeated or
   missing number, and any change of field order. These are discontinuities,
   and each is a finding.

**No frame is discarded silently.** Lead-in frames are scored for their VBI
codes only, because §10.1.1 defines them as preceding the active programme.
Programme frames are scored on every metric.

### Within a frame

Both files follow the CVBS stored-origin rule: sample 0 of every line is the
first sampling instant at or after 0H. Lines are therefore compared sample
index for sample index, with no resampling.

A sub-sample 0H offset between the files is **measured**, not corrected
(timing metrics below). The level and response metrics are insensitive to it:
level windows are taken away from transitions, and chroma phase is measured
against the same line's own burst.

## Measurements

All measurements are differences or ratios between the decoded file and the
truth over the same window, converted to IRE with each file's own `.meta`
levels ([S1](disc-model/s01-cvbs-input.md)).

The window positions within each VITS line come from the signal definitions
(VIRS: FCC 73-699; NTC-7: NTC Report No. 7; IEC 60857 Figures 6b and 7), as
implemented by the pinned videosynth. The comparator,
[`scripts/cvbs_compare.py`](../../scripts/README.md), holds the window table
and cites the definition for each window. Every window is kept at least
0.4 µs plus the transition time away from a transition, and a level window
spans a whole number of subcarrier cycles, so its mean holds no chroma.

| # | Line(s) | Measurement | Method | Threshold | Reason for the threshold |
|---|---------|-------------|--------|-----------|--------------------------|
| L1 | all | Blanking level | mean of the back porch after the burst | ±1 IRE | Plan Task 1.5. Ten times the 0.1 IRE the model's own error budget allows (S7) |
| L2 | all | Sync tip level | mean of the sync tip centre | ±1 IRE | as L1 |
| L3 | 19, 282 (VIRS) | Black reference, luminance reference (IEC 60857 Figure 6b: about 7.5 and 50 IRE; the pinned videosynth renders 0 and 46 IRE, on a 68 IRE chroma-reference pedestal: [videosynth#31](https://github.com/decode-orc/videosynth/issues/31)) | window means | ±1 IRE | as L1 |
| L4 | 20 (NTC-7 composite) | White bar (100 IRE) | window mean | ±1 IRE | as L1 |
| L5 | 20 | Staircase steps | each step's mean | ±1 IRE | as L1 |
| C1 | 19, 282 | VIRS chroma reference amplitude | least-squares sinusoid at fsc; ratio decoded / truth | ±2 % | Plan Task 1.5 |
| C2 | 19, 282 | VIRS chroma phase | phase relative to the line's burst; difference | ±1° | Plan Task 1.5 |
| C3 | 20 | Staircase chroma amplitude and phase (differential gain and phase) | as C1/C2, per step | ±2 %, ±1° | as C1/C2 |
| F1 | 283 (NTC-7 combination) | Multiburst packets, 0.5–4.2 MHz | least-squares sinusoid amplitude per packet; ratio in dB | ±0.5 dB to 3.58 MHz; reported at 4.2 MHz | The disc's pre-emphasis and the decoder's de-emphasis are complementary, and the decoder compensates the readout MTF that the RF carries (see below) |
| F2 | 20 | 2T pulse | peak amplitude ratio; half-amplitude duration difference | reported; reported | The truth's 2T pulse is sampled from a sin² at 4fsc without band-limiting, so it has energy to 7 MHz. Any decoder that limits the video band changes its peak by several percent (the reference decoder, flat to 4.6 MHz: +4.7 %), so no amplitude threshold is derivable (found in Task 7.5) |
| F3 | 20 | 12.5T pulse | least-squares fit of the modulated-pulse model (a sin² luma pulse plus a sin²-modulated subcarrier, each with its own amplitude, centre and duration); chroma-to-luma gain ratio and delay difference | ±2 %; ±15 ns | Delay: the §9.1.7 tolerance at 3.58 MHz. The disc's pre-distortion is specified to ±15 ns there, so a decoder that applies the matching playback equalisation leaves a residual within it |
| T1 | all | 0H position | 50 % point of the leading sync edge, sub-sample interpolated; difference | reported | No derivable threshold yet; set from the carrier-diagnostic baseline in Task 7.4 |
| T2 | all | Burst phase relative to 0H (SC/H) | sinusoid fit on the burst | reported | as T1 |
| V1 | 10, 273 | FM picture number | decode per §10.2 | 100 % correct | Codes are digital; any error is a failure |
| V2 | 11, 274 | White flag presence | §10.2.4 | 100 % correct | as V1 |
| V3 | 16–18, 279–281 | 24-bit biphase codes | decode per §10.1 | 100 % correct | as V1 |
| P1 | active lines | Composite picture difference | RMS over each active line, per frame | reported | Combines all the above; used for triage, not judged on its own |

### Readout MTF and decoder settings

The RF keeps the S11 readout MTF, as a capture from a player's test point
does. At the inner CAV radius, that MTF attenuates the upper FM sideband far
more than the lower one. Uncompensated, chroma would demodulate about
+2.7 dB high at 3.58 MHz at 55 mm (estimate in the
[S11 note](disc-model/s11-optical-readout.md#open-items)), and under 0.2 dB
at 145 mm. ld-decode compensates the MTF itself (`-m/--MTF`). So at 55 mm
and on CLV, F1, C1, C3 and the F3 gain ratio test **the decoder's MTF
compensation against the S11 model**, not just the disc.

A failure there has two candidate causes. One is the decoder's MTF model.
The other is S11's linear readout model, an approximation that plan
Task 10.1 replaces. The bisection is a diagnostic run:

- RF with `player_front_end.mtf_equaliser: inverse` (the S11 MTF removed up
  to the optical cut-off), decoded with `-m 0`
  ([`ntsc_round_trip_cav_inner_equalised.yaml`](../../tests/functional/projects/ntsc_round_trip_cav_inner_equalised.yaml)).

If that run passes and the normal run fails, the disagreement is between the
decoder's MTF compensation and S11. It is recorded as a decoder finding only
if S11 is also supported by the Figure 18 HFMI check and its sources.
Otherwise it is a model finding for Task 10.1.

**Limit of the bisection run (found in Task 7.5).** The equaliser is the
exact inverse only below 0.833·fc and tapers to zero by fc
([S12](disc-model/s12-player-front-end.md)). At 55 mm that is 10.9 to
13.09 MHz, which holds the FM upper sideband of the chroma (carrier plus
3.58 MHz: 11.7–12.5 MHz on the VITS). So at the inner radius the equalised
RF has lost the upper chroma sideband, and the run cannot test chroma.
Where every sideband lies below 0.833·fc·v the run is valid. For chroma at
55 mm the bisection is instead the carrier diagnostic, which has no MTF at
all: a failure that the carrier diagnostic reproduces is not caused by S11.

**ld-decode settings.** Disc baselines use `ld-decode --NTSC` with its
defaults. RF that carries no MTF needs `-m 0`: the carrier-diagnostic
baseline (no disc stages) and the diagnostic equaliser run. The carrier
diagnostic also checks that `-m 0` gives a flat channel; if F1 fails there,
the decoder settings are triaged before any disc baseline is judged. The
ld-decode command line is recorded with each result.

## Results

For each baseline (carrier diagnostic; CAV at 55 mm; CAV at 145 mm; CLV), the
comparator writes one table with, for every measurement:

- minimum, mean and maximum over the scored frames;
- the number of frames outside the threshold;
- the verdict (confirmed or refuted).

Task 7.4 copies these tables below, with the tool versions and the resolved
projects. `scripts/round_trip.py` regenerates them (`build/round_trip/results.md`).

### Baseline results (plan Task 7.4)

**Tools.** ld-creator `c647379` with the Phase 7 working tree, its DdD
filter fitted to the measured board (S13 note, "The fit"); videosynth
`a995726` (flake pin `a9957262e9aa23ffa5d89cb2a66760506db48462`); ld-decode
`8.0.0pre1+git.d6055b3` (flake pin
`d6055b303f67869d005a23619e886453ff73130d`).

**Resolved projects.** Each baseline's committed project
([`tests/functional/projects/`](../../tests/functional/projects/)) with
every other key at its ideal default: S3 on with the §9.1.7 table;
pre-emphasis 320/120 ns; 8.1 MHz blanking carrier, 1.7 MHz deviation; pitch
1.6667 µm; NA 0.40, rim intensity 0.23, HFMI 0.2714; `iec_reference` front
end with no equaliser; `domesday_duplicator` capture at 0.75 of full scale.
The truth's SHA-256 is
`1568e34684699dba36d4ac6b328bd062e86d34442b3e24013cbdb0a417546713`, and the
RF's:

| Baseline | Project | ld-decode | RF SHA-256 |
|----------|---------|-----------|------------|
| Carrier diagnostic | `ntsc_round_trip_carrier.yaml` | `ld-decode --NTSC -m 0` | `54fe522f354b29381e8206ff4b8be877ef0eb8fe0389c1d25c8af2280431395c` |
| Ideal disc, CAV, 55 mm | `ntsc_round_trip_cav_inner.yaml` | `ld-decode --NTSC` | `1a61a328fed8fd281a9d9a7f3d3f0ee05611a0557aad594535e94a64133dd9b3` |
| Ideal disc, CAV, 144.83–145 mm | `ntsc_round_trip_cav_outer.yaml` | `ld-decode --NTSC` | `ff4cb4b70c7c341446cf3a993f5a7a87a5c5b37280ed61b594492aeac370a018` |
| Ideal disc, CLV, 10.1 m/s from 55 mm | `ntsc_round_trip_clv.yaml` | `ld-decode --NTSC` | `8495347452097ce310c68c3a5f6772a036e86402a7125c7b89fccf75f76e7274` |

**Two decoders.** Every RF is decoded by the pinned ld-decode and by
`scripts/reference_demod.py`, a demodulator written from IEC 60857 alone.
The reference decoder applies the §9.1.7 playback equaliser and, for disc
RF, divides out the S11 readout transfer. It does not compensate the DdD
capture filter, so on the disc baselines it shows that filter's effect
rather than a decode of the disc.

**Frames.** ld-decode writes 97 of the 100 frames for every baseline: it
leaves out lead-in frames 0 and 1 and the last programme frame (picture 50).
All 97 pair with the truth: 48 lead-in frames by position and 49 programme
frames (pictures 1–49) by picture number. No frame lacks a valid number,
repeats one or changes field order. Lead-in frames are scored on V1–V3 only,
and programme frames on everything, so "frames outside" counts out of 49.
The reference decoder writes all 100 frames.

**Capture level.** Each disc run measures its captured RF in a first pass
and scales the peak-to-peak to 75 % of the converter's range, as the DdD
guide sets a capture up ([S13](disc-model/s13-rf-capture.md), "Level"). No
run clips.

**Summary** (verdict, with the mean in the measurement's unit):

| # | Threshold | ld-decode, Carrier | ld-decode, CAV 55 mm | ld-decode, CAV 145 mm | ld-decode, CLV | Reference, carrier | Reference, CAV 145 mm (DdD) |
|---|-----------|---|---|---|---|---|---|
| L1 | ±1 IRE | confirmed (+0.05) | confirmed (+0.06) | confirmed (+0.06) | confirmed (+0.06) | confirmed (−0.00) | confirmed (+0.00) |
| L2 | ±1 IRE | confirmed (+0.04) | confirmed (+0.05) | confirmed (+0.04) | confirmed (+0.05) | confirmed (−0.00) | confirmed (−0.00) |
| L3b | ±1 IRE | confirmed (+0.04) | confirmed (+0.06) | confirmed (+0.05) | confirmed (+0.06) | confirmed (+0.00) | confirmed (−0.00) |
| L3l | ±1 IRE | confirmed (+0.05) | confirmed (+0.06) | confirmed (+0.06) | confirmed (+0.06) | confirmed (+0.00) | confirmed (+0.00) |
| L4 | ±1 IRE | confirmed (+0.06) | confirmed (+0.07) | confirmed (+0.06) | confirmed (+0.07) | confirmed (+0.00) | confirmed (+0.00) |
| L5 | ±1 IRE | confirmed (+0.07) | confirmed (+0.07) | confirmed (+0.07) | confirmed (+0.07) | confirmed (+0.01) | confirmed (−0.00) |
| C1 | ±2 % | refuted (+15.60) | refuted (+9.46) | refuted (+11.31) | refuted (+6.87) | confirmed (+0.10) | refuted (−15.16) |
| C2 | ±1 deg | refuted (+1.97) | refuted (+0.77) | refuted (+0.86) | refuted (+0.92) | confirmed (+0.04) | refuted (−2.08) |
| C3a | ±2 % | refuted (+14.99) | refuted (+10.98) | refuted (+12.05) | refuted (+9.57) | confirmed (+0.07) | refuted (−15.26) |
| C3p | ±1 deg | refuted (+1.37) | refuted (+0.84) | refuted (+0.86) | refuted (+0.88) | confirmed (+0.10) | refuted (−1.10) |
| F1 0.5 | ±0.5 dB | confirmed (+0.09) | confirmed (+0.02) | confirmed (+0.03) | confirmed (+0.02) | confirmed (+0.00) | confirmed (−0.07) |
| F1 1 | ±0.5 dB | confirmed (+0.16) | confirmed (−0.07) | confirmed (−0.07) | confirmed (−0.07) | confirmed (+0.01) | confirmed (−0.27) |
| F1 2 | ±0.5 dB | confirmed (+0.15) | confirmed (−0.20) | confirmed (−0.17) | confirmed (−0.19) | confirmed (+0.01) | refuted (−0.89) |
| F1 3 | ±0.5 dB | refuted (+1.02) | confirmed (+0.33) | confirmed (+0.23) | confirmed (+0.29) | confirmed (+0.01) | refuted (−1.45) |
| F1 3.58 | ±0.5 dB | refuted (+1.28) | refuted (+0.85) | refuted (+0.91) | refuted (+0.62) | confirmed (+0.02) | refuted (−1.49) |
| F1 4.2 | reported | +0.18 | +0.61 | +0.82 | +0.64 | +0.04 | −1.21 |
| F2a | reported | +7.49 | +7.85 | +4.94 | +7.22 | +4.71 | −2.97 |
| F2d | reported | −14.12 | −20.90 | −13.60 | −19.89 | −12.24 | −2.47 |
| F3g | ±2 % | refuted (+13.25) | refuted (+10.02) | refuted (+9.42) | refuted (+7.90) | confirmed (−0.05) | refuted (−15.67) |
| F3d | ±15 ns | refuted (+35.39) | refuted (+12.77) | refuted (+17.43) | refuted (+13.43) | confirmed (+2.91) | refuted (−16.82) |
| T1 | reported | −7.45 | −4.09 | −4.95 | −4.04 | +0.57 | +59.43 |
| T2 | reported | −8.69 | −4.80 | −5.91 | −4.69 | −1.61 | +0.94 |
| V1 | ±0 lines | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) |
| V2 | ±0 lines | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) |
| V3 | ±0 lines | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) |
| P1 | reported | +2.16 | +1.57 | +1.61 | +1.44 | +0.80 | +12.53 |

ld-decode recovers the levels (L1–L5) and the VBI codes (V1–V3) on every
baseline, within 0.14 IRE and with every code exact. Its chroma measurements
(C1, C3a, C3p, F3g) are refuted on every baseline. The reference decoder
confirms every judged measurement on the carrier diagnostic, so the FM
signal carries the truth by the standard. 0H (T1) and SC/H (T2) are
reported.

#### Carrier diagnostic

ld-decode command: `ld-decode --NTSC -m 0 carrier.s16 carrier_decoded`. Verdict: **refuted**.

| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.030 | +0.051 | +0.119 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | +0.005 | +0.039 | +0.079 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | +0.027 | +0.044 | +0.064 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | +0.043 | +0.054 | +0.066 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | +0.046 | +0.055 | +0.064 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | +0.033 | +0.071 | +0.115 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | +15.474 | +15.595 | +15.692 | 49 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | +1.771 | +1.970 | +2.141 | 49 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | +13.701 | +14.986 | +16.030 | 49 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -0.240 | +1.371 | +2.279 | 49 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | +0.084 | +0.092 | +0.098 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | +0.147 | +0.157 | +0.171 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | +0.138 | +0.151 | +0.161 | 0 | confirmed |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | +1.002 | +1.016 | +1.033 | 49 | refuted |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | +1.261 | +1.283 | +1.297 | 49 | refuted |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | +0.159 | +0.185 | +0.199 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | +7.257 | +7.490 | +7.635 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | -14.425 | -14.122 | -13.793 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | +13.009 | +13.245 | +13.438 | 49 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | +34.274 | +35.386 | +36.766 | 49 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | -8.349 | -7.453 | -6.391 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -9.726 | -8.687 | -7.749 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.093 | +2.162 | +4.535 | — | reported |
| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.060 | -0.000 | +0.060 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | -0.035 | -0.002 | +0.020 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | -0.006 | +0.001 | +0.011 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | -0.007 | +0.001 | +0.011 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | -0.005 | +0.000 | +0.004 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | -0.056 | +0.009 | +0.051 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | +0.053 | +0.102 | +0.163 | 0 | confirmed |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | -0.085 | +0.041 | +0.232 | 0 | confirmed |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | -0.569 | +0.072 | +0.634 | 0 | confirmed |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -0.352 | +0.098 | +0.535 | 0 | confirmed |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | -0.006 | +0.001 | +0.008 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.004 | +0.007 | +0.024 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.008 | +0.008 | +0.024 | 0 | confirmed |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -0.010 | +0.008 | +0.031 | 0 | confirmed |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | +0.003 | +0.020 | +0.038 | 0 | confirmed |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | +0.016 | +0.039 | +0.054 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | +4.643 | +4.714 | +4.844 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | -12.657 | -12.243 | -12.036 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | -0.274 | -0.047 | +0.206 | 0 | confirmed |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | +1.768 | +2.914 | +4.014 | 0 | confirmed |
| T1 | 0H position difference, all normal-sync lines | ns | reported | +0.043 | +0.570 | +1.172 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -2.493 | -1.609 | -0.765 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.053 | +0.803 | +2.151 | — | reported |

Discontinuities: lead-in frames 0-1 not decoded (before the first decoded frame); pictures 50-50 not decoded (after the last decoded frame).

#### Ideal disc, CAV, 55 mm

ld-decode command: `ld-decode --NTSC cav_inner.s16 cav_inner_decoded`. Verdict: **refuted**.

| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.030 | +0.056 | +0.134 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | +0.010 | +0.046 | +0.079 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | +0.042 | +0.058 | +0.072 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | +0.051 | +0.063 | +0.077 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | +0.057 | +0.068 | +0.074 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | -0.011 | +0.066 | +0.128 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | +9.145 | +9.458 | +9.752 | 49 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | +0.414 | +0.770 | +1.192 | 5 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | +8.520 | +10.985 | +14.360 | 49 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -0.719 | +0.837 | +2.400 | 49 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | +0.010 | +0.021 | +0.034 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.081 | -0.066 | -0.053 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.234 | -0.200 | -0.176 | 0 | confirmed |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | +0.288 | +0.332 | +0.366 | 0 | confirmed |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | +0.809 | +0.850 | +0.900 | 49 | refuted |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | +0.580 | +0.615 | +0.640 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | +7.455 | +7.846 | +8.181 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | -21.401 | -20.900 | -20.327 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | +9.481 | +10.024 | +10.547 | 49 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | +10.130 | +12.774 | +16.368 | 3 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | -5.690 | -4.085 | -1.118 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -6.709 | -4.795 | -0.882 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.139 | +1.565 | +3.037 | — | reported |
| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.253 | +0.013 | +0.283 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | -0.084 | +0.001 | +0.268 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | -0.021 | -0.000 | +0.018 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | -0.013 | +0.000 | +0.016 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | -0.011 | +0.002 | +0.015 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | -0.126 | -0.011 | +0.201 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | -18.699 | -16.085 | -13.866 | 50 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | -4.982 | -2.090 | +0.878 | 49 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | -60.366 | -24.748 | -3.943 | 50 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -7.068 | -0.119 | +9.233 | 50 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | -0.090 | -0.071 | -0.055 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.295 | -0.264 | -0.232 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.969 | -0.908 | -0.827 | 50 | refuted |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -1.624 | -1.415 | -1.231 | 50 | refuted |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | -2.014 | -1.659 | -1.213 | 50 | refuted |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | -6.713 | -6.526 | -6.356 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | -10.589 | -9.154 | -7.963 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | +14.851 | +18.681 | +23.083 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | -23.245 | -19.890 | -17.242 | 50 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | -37.753 | -18.993 | +7.527 | 35 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | +48.843 | +59.206 | +66.420 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -14.221 | +0.654 | +11.374 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.598 | +12.154 | +27.152 | — | reported |

Discontinuities: lead-in frames 0-1 not decoded (before the first decoded frame); pictures 50-50 not decoded (after the last decoded frame).

#### Ideal disc, CAV, 144.83–145 mm

ld-decode command: `ld-decode --NTSC cav_outer.s16 cav_outer_decoded`. Verdict: **refuted**.

| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.015 | +0.058 | +0.119 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | +0.010 | +0.044 | +0.079 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | +0.035 | +0.054 | +0.069 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | +0.047 | +0.060 | +0.068 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | +0.056 | +0.064 | +0.071 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | +0.011 | +0.070 | +0.121 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | +11.206 | +11.314 | +11.441 | 49 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | +0.676 | +0.865 | +1.074 | 7 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | +10.077 | +12.046 | +14.246 | 49 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -0.506 | +0.864 | +1.779 | 49 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | +0.019 | +0.025 | +0.032 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.079 | -0.068 | -0.055 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.184 | -0.170 | -0.155 | 0 | confirmed |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | +0.218 | +0.232 | +0.251 | 0 | confirmed |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | +0.893 | +0.911 | +0.932 | 49 | refuted |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | +0.795 | +0.816 | +0.837 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | +4.760 | +4.939 | +5.151 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | -14.114 | -13.598 | -13.022 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | +8.955 | +9.425 | +9.837 | 49 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | +15.788 | +17.432 | +19.295 | 49 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | -5.993 | -4.950 | -3.986 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -7.071 | -5.911 | -4.764 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.103 | +1.615 | +3.414 | — | reported |
| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.074 | +0.003 | +0.060 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | -0.040 | -0.002 | +0.030 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | -0.010 | -0.000 | +0.010 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | -0.008 | +0.001 | +0.010 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | -0.010 | +0.000 | +0.010 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | -0.030 | -0.002 | +0.033 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | -15.234 | -15.161 | -15.089 | 50 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | -2.387 | -2.082 | -1.819 | 50 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | -16.591 | -15.263 | -12.839 | 50 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -3.761 | -1.097 | +1.181 | 50 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | -0.078 | -0.069 | -0.061 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.279 | -0.269 | -0.258 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.912 | -0.895 | -0.883 | 50 | refuted |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -1.468 | -1.449 | -1.426 | 50 | refuted |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | -1.509 | -1.486 | -1.466 | 50 | refuted |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | -1.239 | -1.209 | -1.176 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | -3.121 | -2.971 | -2.717 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | -3.080 | -2.474 | -1.834 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | -16.119 | -15.665 | -15.223 | 50 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | -18.924 | -16.823 | -14.284 | 46 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | +58.799 | +59.430 | +60.218 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -0.077 | +0.943 | +2.033 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.068 | +12.534 | +27.382 | — | reported |

Discontinuities: lead-in frames 0-1 not decoded (before the first decoded frame); pictures 50-50 not decoded (after the last decoded frame).

#### Ideal disc, CLV, 10.1 m/s from 55 mm

ld-decode command: `ld-decode --NTSC clv.s16 clv_decoded`. Verdict: **refuted**.

| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.030 | +0.057 | +0.298 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | -0.010 | +0.046 | +0.084 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | +0.043 | +0.057 | +0.072 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | +0.051 | +0.063 | +0.076 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | +0.056 | +0.068 | +0.080 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | -0.011 | +0.068 | +0.134 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | +6.759 | +6.870 | +6.963 | 49 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | +0.271 | +0.919 | +1.364 | 27 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | +5.319 | +9.574 | +14.990 | 49 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -0.769 | +0.876 | +2.385 | 49 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | +0.013 | +0.023 | +0.034 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.080 | -0.068 | -0.056 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.214 | -0.190 | -0.163 | 0 | confirmed |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | +0.215 | +0.292 | +0.351 | 0 | confirmed |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | +0.570 | +0.619 | +0.660 | 49 | refuted |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | +0.613 | +0.636 | +0.652 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | +6.920 | +7.218 | +7.483 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | -20.340 | -19.887 | -19.202 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | +7.383 | +7.897 | +8.474 | 49 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | +9.010 | +13.434 | +17.776 | 9 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | -11.412 | -4.045 | -2.218 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -14.139 | -4.695 | -2.201 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.171 | +1.443 | +2.957 | — | reported |
| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.253 | +0.015 | +0.298 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | -0.079 | -0.003 | +0.263 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | -0.011 | +0.002 | +0.014 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | -0.013 | +0.000 | +0.014 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | -0.014 | +0.002 | +0.014 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | -0.171 | -0.040 | +0.100 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | -54.577 | -54.442 | -54.317 | 50 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | +0.766 | +4.052 | +6.676 | 50 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | -54.010 | -34.272 | -10.518 | 50 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -9.982 | +0.484 | +8.671 | 50 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | -0.093 | -0.071 | -0.050 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.309 | -0.273 | -0.247 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.976 | -0.922 | -0.860 | 50 | refuted |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -1.775 | -1.466 | -1.223 | 50 | refuted |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | -5.453 | -4.985 | -4.499 | 50 | refuted |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | -6.966 | -6.756 | -6.620 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | -13.104 | -12.327 | -11.280 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | +25.328 | +28.694 | +31.272 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | -40.931 | -38.151 | -35.719 | 50 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | -66.116 | -31.198 | +2.336 | 44 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | +47.313 | +58.950 | +67.713 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -14.876 | +0.719 | +13.690 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.541 | +11.388 | +25.060 | — | reported |

Discontinuities: lead-in frames 0-1 not decoded (before the first decoded frame); pictures 50-50 not decoded (after the last decoded frame).

### Triage (plan Task 7.5)

Every refuted prediction is classified below. The evidence, and what would
settle each unattributed discrepancy, are in
[ld-decode-findings.md](ld-decode-findings.md).

| Refuted | Baselines | Class | Evidence |
|---------|-----------|-------|----------|
| C1, C3a, F1 3.58 MHz, F3g (chroma level and response) | disc baselines | Measurement (the truth); C1's residual at 145 mm and on CLV unattributed (U1) | The truth's colour burst lasts 7.25 cycles from 5.63 µs, short of the 9 ± 1 cycles from 5.3 µs that IEC 60857 §9.1.2 and BT.470-6 require (videosynth#32). ld-decode measures it low and over-corrects chroma. With the burst replaced by the standard one, and nothing else changed, F1 3.58 MHz and F3g confirm on all three disc baselines and C1 reads +0.7, +2.5 and −2.2 % (from +9.5, +11.3 and +6.9 %); C3a's spread remains (U4; ld-decode-findings.md, "The colour burst") |
| C1, C3a, F1 3.58 MHz, F3g (chroma level and response) | carrier only | Unattributed (U1, U3) | The reference decoder recovers chroma on the carrier diagnostic within 0.1 %, so the FM signal is right. ld-decode reads +15.6 % there with its burst-driven correction (−9.3 % without). ld-decode is not built for MTF-free RF |
| F3d (chroma-to-luma delay) | all four (ld-decode) | Unattributed (U2) | The reference decoder with the §9.1.7 playback equaliser reads +2.9 ns on the carrier diagnostic; ld-decode adds about 116 ns against the 80 ns the pre-distortion presumes. On a real disc (GGV1069), whose RF timing matches ld-creator's, ld-decode shows a similar delay, but that disc's own VITS timing is unknown |
| C2, C3p, and C3a's spread on disc RF (chroma phase, differential phase and gain) | all | Unattributed (U4) | The reference decoder reads +0.0° and within ±0.5° on the carrier diagnostic |
| F2a (2T amplitude) | — | Measurement (corrected) | The truth's 2T is not band-limited, so any band-limited decoder misses ±2 % (the reference decoder reads +4.7 %); F2a is now reported, not judged |
| Every chroma row of the MTF bisection run | `cav_inner_equalised` (diagnostic) | Measurement (the run's design) | At 55 mm the S12 equaliser tapers to zero over 10.9–13.09 MHz, where the upper chroma sideband lies |

No discrepancy is a model finding: S2–S5 decode to the truth through the
reference decoder, and S6–S13 do too at 145 mm with the `ideal` capture.
None is a lead-in shortfall: every failing measurement fails on all 49
programme frames. None is a decoder finding: ld-decode is calibrated on
real discs, and the one real capture compared (Pioneer GGV1069) is one
disc's mastering, which cannot show what real discs in general carry (see
"Triage classes" and ld-decode-findings.md, "The real capture"). The
results above use the pinned videosynth, short burst included, until its
burst is corrected and the pin updated.

## Triage classes

| Class | Meaning |
|-------|---------|
| Model | A stage does not conform to its note. Fix the stage, then rerun |
| Lead-in shortfall | Failures are confined to the first programme frames and stop once the decoder has settled. Lengthen the lead-in and rerun |
| Decoder finding | Every stage conforms to its note, the RF is shown to carry the right signal, and real-disc evidence shows that the RF resembles a real disc where the failure arises (a real capture decoded with the same settings settles the same way, or the decoder's behaviour is wrong on real discs too). Recorded with the evidence in `ld-decode-findings.md` |
| Unattributed | Every stage conforms to its note and the RF carries the IEC 60857 signal, but the decoder disagrees, and nothing yet shows whether ld-creator's IEC-nominal disc or the decoder's calibration departs from real discs. The decoder is calibrated on real discs, not on the standard, so conformance to the standard alone does not make the decoder wrong. Held open, with the evidence and what would settle it, in `ld-decode-findings.md` |
| Measurement | The comparator, or the truth it compares against, is wrong. Fix the comparator, and check it against the truth-against-truth and +2 IRE tests (Task 7.2); a truth that departs from its standard is reported against videosynth and shown by a bisection run with the truth corrected |
