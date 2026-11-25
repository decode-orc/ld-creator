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

**Tools.** ld-creator `c647379` with the Phase 7 working tree; videosynth
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
| Ideal disc, CAV, 55 mm | `ntsc_round_trip_cav_inner.yaml` | `ld-decode --NTSC` | `45ca292c3880f7419d416714933e1a4e9091a03d8cc71cdb1108de8a9f978336` |
| Ideal disc, CAV, 144.83–145 mm | `ntsc_round_trip_cav_outer.yaml` | `ld-decode --NTSC` | `851e3d9fea7f118c2f86aa798f896047d7831b592f23adbf6d3b871789d1bba0` |
| Ideal disc, CLV, 10.1 m/s from 55 mm | `ntsc_round_trip_clv.yaml` | `ld-decode --NTSC` | `a2adf9c9005fd0f7694d54a7dedf16ea6afea36cd6bc4a4ec1073ca5cfcc696b` |

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

**Converter clipping.** ld-creator reports clipped RF samples on two disc
runs. At CAV 55 mm, 91 samples clip, all on the broad-pulse lines (4–6,
267–269) 5–7 µs after 0H: the colour burst that `laserdisc.vbi_burst` keeps
there reaches −60 IRE, below sync tip, and the lowest carriers come out
largest, as the [S13](disc-model/s13-rf-capture.md) level rule says. On CLV,
869 samples clip, also at the dark side of pre-emphasised transitions in the
VBI codes and the picture. The CAV 145 mm run does not clip. Every VBI code
still decodes exactly; the headroom question is an S13 open item.

**Summary** (verdict, with the mean in the measurement's unit):

| # | Threshold | ld-decode, Carrier | ld-decode, CAV 55 mm | ld-decode, CAV 145 mm | ld-decode, CLV | Reference, carrier | Reference, CAV 145 mm (DdD) |
|---|-----------|---|---|---|---|---|---|
| L1 | ±1 IRE | confirmed (+0.05) | confirmed (+0.06) | confirmed (+0.06) | confirmed (+0.06) | confirmed (−0.00) | confirmed (+0.01) |
| L2 | ±1 IRE | confirmed (+0.04) | confirmed (+0.05) | confirmed (+0.04) | confirmed (+0.05) | confirmed (−0.00) | confirmed (−0.00) |
| L3b | ±1 IRE | confirmed (+0.04) | confirmed (+0.06) | confirmed (+0.05) | confirmed (+0.06) | confirmed (+0.00) | confirmed (−0.00) |
| L3l | ±1 IRE | confirmed (+0.05) | confirmed (+0.06) | confirmed (+0.06) | confirmed (+0.06) | confirmed (+0.00) | confirmed (−0.00) |
| L4 | ±1 IRE | confirmed (+0.06) | confirmed (+0.07) | confirmed (+0.06) | confirmed (+0.06) | confirmed (+0.00) | confirmed (−0.00) |
| L5 | ±1 IRE | confirmed (+0.07) | confirmed (+0.07) | confirmed (+0.07) | confirmed (+0.07) | confirmed (+0.01) | confirmed (−0.01) |
| C1 | ±2 % | refuted (+15.60) | refuted (+5.61) | refuted (+5.83) | refuted (+5.14) | confirmed (+0.10) | refuted (−18.55) |
| C2 | ±1 deg | refuted (+1.97) | refuted (+1.21) | confirmed (−0.80) | refuted (+1.51) | confirmed (+0.04) | refuted (−2.80) |
| C3a | ±2 % | refuted (+14.99) | refuted (+7.04) | refuted (+8.29) | refuted (+6.38) | confirmed (+0.07) | refuted (−17.92) |
| C3p | ±1 deg | refuted (+1.37) | refuted (+1.15) | refuted (−0.24) | refuted (+1.23) | confirmed (+0.10) | refuted (−1.62) |
| F1 0.5 | ±0.5 dB | confirmed (+0.09) | confirmed (−0.01) | confirmed (+0.03) | confirmed (−0.01) | confirmed (+0.00) | confirmed (−0.06) |
| F1 1 | ±0.5 dB | confirmed (+0.16) | confirmed (−0.19) | confirmed (−0.09) | confirmed (−0.19) | confirmed (+0.01) | confirmed (−0.26) |
| F1 2 | ±0.5 dB | confirmed (+0.15) | confirmed (−0.35) | confirmed (−0.23) | confirmed (−0.34) | confirmed (+0.01) | refuted (−0.99) |
| F1 3 | ±0.5 dB | refuted (+1.02) | confirmed (−0.18) | confirmed (−0.12) | confirmed (−0.21) | confirmed (+0.01) | refuted (−1.75) |
| F1 3.58 | ±0.5 dB | refuted (+1.28) | refuted (+0.51) | refuted (+0.56) | confirmed (+0.42) | confirmed (+0.02) | refuted (−1.79) |
| F1 4.2 | reported | +0.18 | +0.51 | +0.62 | +0.62 | +0.04 | −1.33 |
| F2a | reported | +7.49 | +4.12 | +2.63 | +3.92 | +4.71 | −4.73 |
| F2d | reported | −14.12 | −15.95 | −9.36 | −15.60 | −12.24 | +1.25 |
| F3g | ±2 % | refuted (+13.25) | refuted (+5.92) | refuted (+5.64) | refuted (+4.98) | confirmed (−0.05) | refuted (−18.37) |
| F3d | ±15 ns | refuted (+35.39) | confirmed (+3.95) | refuted (+17.38) | confirmed (+3.04) | confirmed (+2.91) | refuted (−15.30) |
| T1 | reported | −7.45 | −3.48 | −6.29 | −3.40 | +0.57 | +55.16 |
| T2 | reported | −8.69 | −4.12 | −7.68 | −4.00 | −1.61 | −2.50 |
| V1 | ±0 lines | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) |
| V2 | ±0 lines | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) |
| V3 | ±0 lines | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) |
| P1 | reported | +2.16 | +1.37 | +1.54 | +1.27 | +0.80 | +12.34 |

ld-decode recovers the levels (L1–L5) and the VBI codes (V1–V3) on every
baseline, within 0.14 IRE and with every code exact. Its chroma
measurements (C1, C3a, C3p, F3g) are refuted on every baseline. The reference
decoder confirms every judged measurement on the carrier diagnostic, so the
FM signal carries the truth by the standard. 0H (T1) and SC/H (T2) are
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
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.045 | +0.057 | +0.134 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | +0.010 | +0.046 | +0.084 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | +0.039 | +0.057 | +0.068 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | +0.053 | +0.062 | +0.075 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | +0.058 | +0.066 | +0.074 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | -0.022 | +0.067 | +0.121 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | +4.192 | +5.605 | +6.403 | 49 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | +0.831 | +1.207 | +1.523 | 48 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | +3.737 | +7.036 | +14.204 | 49 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -0.551 | +1.153 | +2.397 | 49 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | -0.020 | -0.011 | -0.003 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.209 | -0.192 | -0.177 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.371 | -0.353 | -0.335 | 0 | confirmed |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -0.202 | -0.179 | -0.134 | 0 | confirmed |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | +0.488 | +0.513 | +0.543 | 40 | refuted |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | +0.481 | +0.514 | +0.538 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | +3.884 | +4.118 | +4.443 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | -16.429 | -15.953 | -15.444 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | +5.501 | +5.918 | +6.525 | 49 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | +2.069 | +3.948 | +6.525 | 0 | confirmed |
| T1 | 0H position difference, all normal-sync lines | ns | reported | -4.752 | -3.479 | -2.386 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -5.791 | -4.124 | -2.739 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.117 | +1.372 | +3.036 | — | reported |
| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.238 | +0.019 | +0.298 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | -0.084 | +0.001 | +0.149 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | -0.014 | +0.000 | +0.012 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | -0.012 | +0.002 | +0.019 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | -0.006 | +0.002 | +0.016 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | -0.121 | -0.011 | +0.156 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | -21.307 | -19.689 | -18.151 | 50 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | -5.160 | -2.792 | -0.092 | 50 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | -67.424 | -28.334 | -8.594 | 50 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -2.980 | +1.209 | +14.398 | 50 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | -0.074 | -0.059 | -0.039 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.284 | -0.253 | -0.229 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -1.062 | -0.998 | -0.946 | 50 | refuted |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -1.802 | -1.697 | -1.555 | 50 | refuted |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | -2.380 | -2.074 | -1.793 | 50 | refuted |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | -8.103 | -7.917 | -7.764 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | -12.076 | -11.045 | -10.059 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | +21.968 | +25.200 | +28.897 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | -25.544 | -23.508 | -21.995 | 50 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | -36.256 | -21.730 | -8.567 | 40 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | +45.884 | +54.700 | +60.774 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -13.960 | -3.082 | +4.316 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.439 | +11.625 | +26.284 | — | reported |

Discontinuities: lead-in frames 0-1 not decoded (before the first decoded frame); pictures 50-50 not decoded (after the last decoded frame).

#### Ideal disc, CAV, 144.83–145 mm

ld-decode command: `ld-decode --NTSC cav_outer.s16 cav_outer_decoded`. Verdict: **refuted**.

| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.015 | +0.061 | +0.119 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | +0.010 | +0.045 | +0.079 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | +0.035 | +0.051 | +0.062 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | +0.043 | +0.056 | +0.068 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | +0.054 | +0.063 | +0.074 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | +0.011 | +0.071 | +0.128 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | +5.685 | +5.829 | +5.953 | 49 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | -0.954 | -0.803 | -0.633 | 0 | confirmed |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | +5.296 | +8.288 | +13.949 | 49 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -2.873 | -0.243 | +1.374 | 49 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | +0.018 | +0.025 | +0.035 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.100 | -0.086 | -0.072 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.242 | -0.227 | -0.215 | 0 | confirmed |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -0.142 | -0.123 | -0.103 | 0 | confirmed |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | +0.537 | +0.557 | +0.575 | 49 | refuted |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | +0.598 | +0.620 | +0.648 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | +2.444 | +2.631 | +2.824 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | -9.843 | -9.362 | -8.890 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | +5.201 | +5.644 | +6.175 | 49 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | +15.975 | +17.382 | +19.001 | 49 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | -7.296 | -6.295 | -5.372 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -8.980 | -7.679 | -6.583 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.100 | +1.537 | +4.024 | — | reported |
| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.045 | +0.005 | +0.060 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | -0.030 | -0.000 | +0.030 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | -0.008 | -0.000 | +0.010 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | -0.006 | -0.000 | +0.008 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | -0.007 | -0.000 | +0.005 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | -0.045 | -0.008 | +0.020 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | -18.641 | -18.545 | -18.460 | 50 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | -3.027 | -2.799 | -2.562 | 50 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | -19.455 | -17.920 | -15.997 | 50 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -4.657 | -1.625 | +0.995 | 50 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | -0.066 | -0.058 | -0.053 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.268 | -0.257 | -0.246 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -1.009 | -0.995 | -0.975 | 50 | refuted |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -1.762 | -1.746 | -1.725 | 50 | refuted |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | -1.815 | -1.793 | -1.778 | 50 | refuted |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | -1.353 | -1.326 | -1.297 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | -4.895 | -4.732 | -4.523 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | +0.533 | +1.253 | +1.860 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | -18.900 | -18.371 | -17.784 | 50 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | -17.525 | -15.302 | -13.026 | 30 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | +54.279 | +55.157 | +56.075 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -3.734 | -2.503 | -1.216 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.061 | +12.340 | +26.556 | — | reported |

Discontinuities: lead-in frames 0-1 not decoded (before the first decoded frame); pictures 50-50 not decoded (after the last decoded frame).

#### Ideal disc, CLV, 10.1 m/s from 55 mm

ld-decode command: `ld-decode --NTSC clv.s16 clv_decoded`. Verdict: **refuted**.

| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.074 | +0.057 | +0.134 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | -0.005 | +0.046 | +0.084 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | +0.042 | +0.057 | +0.069 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | +0.047 | +0.062 | +0.075 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | +0.046 | +0.064 | +0.072 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | -0.022 | +0.066 | +0.121 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | +4.998 | +5.139 | +5.242 | 49 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | +1.150 | +1.511 | +1.883 | 49 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | +3.280 | +6.380 | +10.156 | 49 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -0.851 | +1.234 | +2.691 | 49 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | -0.021 | -0.010 | -0.001 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.204 | -0.195 | -0.184 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.368 | -0.340 | -0.318 | 0 | confirmed |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -0.247 | -0.210 | -0.181 | 0 | confirmed |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | +0.382 | +0.416 | +0.447 | 0 | confirmed |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | +0.600 | +0.616 | +0.630 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | +3.558 | +3.920 | +4.102 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | -15.944 | -15.604 | -15.013 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | +4.663 | +4.982 | +5.250 | 49 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | +0.579 | +3.042 | +5.603 | 0 | confirmed |
| T1 | 0H position difference, all normal-sync lines | ns | reported | -5.881 | -3.398 | -0.217 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -7.174 | -3.999 | +0.132 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.124 | +1.274 | +3.001 | — | reported |
| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.179 | +0.019 | +0.223 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | -0.069 | -0.004 | +0.069 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | -0.010 | +0.001 | +0.017 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | -0.008 | +0.000 | +0.013 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | -0.004 | +0.004 | +0.013 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | -0.171 | -0.047 | +0.038 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | -61.854 | -61.771 | -61.679 | 50 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | +10.025 | +11.906 | +14.276 | 50 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | -61.834 | -38.980 | -13.376 | 50 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -6.844 | +3.244 | +15.537 | 50 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | -0.083 | -0.059 | -0.047 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.285 | -0.255 | -0.233 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -1.063 | -1.020 | -0.973 | 50 | refuted |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -1.883 | -1.741 | -1.593 | 50 | refuted |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | -6.455 | -5.987 | -5.661 | 50 | refuted |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | -8.262 | -8.169 | -8.051 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | -15.641 | -14.650 | -14.053 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | +35.546 | +37.897 | +41.342 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | -45.106 | -43.502 | -41.937 | 50 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | -98.212 | -70.343 | -49.677 | 50 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | +43.856 | +54.212 | +61.400 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -17.791 | -3.247 | +6.698 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.379 | +10.664 | +23.814 | — | reported |

Discontinuities: lead-in frames 0-1 not decoded (before the first decoded frame); pictures 50-50 not decoded (after the last decoded frame).

### Triage (plan Task 7.5)

Every refuted prediction is classified below. The evidence, and what would
settle each unattributed discrepancy, are in
[ld-decode-findings.md](ld-decode-findings.md).

| Refuted | Baselines | Class | Evidence |
|---------|-----------|-------|----------|
| C1, C3a, F1 3–3.58 MHz, F3g (chroma level and response) | all (F1 3.58 MHz: carrier, CAV) | Unattributed (U1, U3) | The reference decoder recovers chroma on the carrier diagnostic within 0.1 %, so the FM signal is right; ld-decode reads +15.6 % with its burst-driven correction, −9.3 % without. ld-decode is not built for MTF-free RF, and on disc RF the result depends on the DdD filter model (Task 5.3) and S11 near cut-off |
| F3d (chroma-to-luma delay) | carrier, CAV 145 mm | Unattributed (U2) | The reference decoder with the §9.1.7 playback equaliser reads +2.9 ns on the carrier diagnostic; ld-decode adds about 116 ns against the 80 ns the pre-distortion presumes |
| C2, C3p (chroma phase, differential phase) | all (C2: carrier, 55 mm, CLV) | Unattributed (U4) | The reference decoder reads +0.0° and within ±0.5° on the carrier diagnostic |
| F2a (2T amplitude) | — | Measurement (corrected) | The truth's 2T is not band-limited, so any band-limited decoder misses ±2 % (the reference decoder reads +4.7 %); F2a is now reported, not judged |
| Every chroma row of the MTF bisection run | `cav_inner_equalised` (diagnostic) | Measurement (the run's design) | At 55 mm the S12 equaliser tapers to zero over 10.9–13.09 MHz, where the upper chroma sideband lies |

No discrepancy is a model finding: S2–S5 decode to the truth through the
reference decoder, and S6–S13 do too at 145 mm with the `ideal` capture.
None is a lead-in shortfall: every failing measurement fails on all 49
programme frames. None is yet a decoder finding: ld-decode is calibrated on
real discs, so attributing a discrepancy to it needs a real capture (see
"Triage classes").

## Triage classes

| Class | Meaning |
|-------|---------|
| Model | A stage does not conform to its note. Fix the stage, then rerun |
| Lead-in shortfall | Failures are confined to the first programme frames and stop once the decoder has settled. Lengthen the lead-in and rerun |
| Decoder finding | Every stage conforms to its note, the RF is shown to carry the right signal, and real-disc evidence shows that the RF resembles a real disc where the failure arises (a real capture decoded with the same settings settles the same way, or the decoder's behaviour is wrong on real discs too). Recorded with the evidence in `ld-decode-findings.md` |
| Unattributed | Every stage conforms to its note and the RF carries the IEC 60857 signal, but the decoder disagrees, and nothing yet shows whether ld-creator's IEC-nominal disc or the decoder's calibration departs from real discs. The decoder is calibrated on real discs, not on the standard, so conformance to the standard alone does not make the decoder wrong. Held open, with the evidence and what would settle it, in `ld-decode-findings.md` |
| Measurement | The comparator is wrong. Fix it, and check it against the truth-against-truth and +2 IRE tests (Task 7.2) |
