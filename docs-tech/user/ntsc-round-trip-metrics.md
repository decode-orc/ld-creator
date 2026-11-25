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
- **FM code in the lead-in.** §10.2.3 puts FM picture number 0 on lines 10
  and 273 during the lead-in, and the lead-in section carries it (videosynth
  has generated it there since
  [videosynth#31](https://github.com/decode-orc/videosynth/issues/31) was
  fixed). V1 scores the lead-in frames too, so it checks the decoded code
  there.

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
| L3 | 19, 282 (VIRS) | Black reference (48–60 µs, 7.5 IRE) and luminance reference (36–48 µs, 50 IRE), after a 70 IRE chroma-reference pedestal (12–36 µs; IEC 60857 Figure 6b) | window means | ±1 IRE | as L1 |
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

**Tools.** ld-creator `cea9361` with the working tree, its DdD filter
fitted to the measured board (S13 note, "The fit"); videosynth `cd9e698`
(flake pin `cd9e698b08965f0d6dd2195c456e121f1dbdc53b`), which has the fixes
for [videosynth#31](https://github.com/decode-orc/videosynth/issues/31),
[#32](https://github.com/decode-orc/videosynth/issues/32) and
[#33](https://github.com/decode-orc/videosynth/issues/33); ld-decode
`8.0.0pre1+git.d6055b3` (flake pin
`d6055b303f67869d005a23619e886453ff73130d`).

**Resolved projects.** Each baseline's committed project
([`tests/functional/projects/`](../../tests/functional/projects/)) with
every other key at its ideal default: S3 on with the §9.1.7 table;
pre-emphasis 320/120 ns; 8.1 MHz blanking carrier, 1.7 MHz deviation; pitch
1.6667 µm; NA 0.40, rim intensity 0.23, HFMI 0.2714; `iec_reference` front
end with no equaliser; `domesday_duplicator` capture at 0.75 of full scale.
The truth's SHA-256 is
`89fcdf01493ae2d2f1fcebf2f2b3db0acc8f18057b28774a810bfb225193cc24`, and the
RF's:

| Baseline | Project | ld-decode | RF SHA-256 |
|----------|---------|-----------|------------|
| Carrier diagnostic | `ntsc_round_trip_carrier.yaml` | `ld-decode --NTSC -m 0` | `f248fa3b120356a353b7c42dc3a047c603342dfe50b81dbff0c7a10662afc54d` |
| Ideal disc, CAV, 55 mm | `ntsc_round_trip_cav_inner.yaml` | `ld-decode --NTSC` | `ebb415c9a894cc33ff228e63da12f73522f2c44d5d5e77a0b275f075169ab1a4` |
| Ideal disc, CAV, 144.83–145 mm | `ntsc_round_trip_cav_outer.yaml` | `ld-decode --NTSC` | `8378714ef4424fcc979b8860ad261f45ef8eb5d7d1106f01e98b75be7cd4f06b` |
| Ideal disc, CLV, 10.1 m/s from 55 mm | `ntsc_round_trip_clv.yaml` | `ld-decode --NTSC` | `c136539ed6911383fbcba52888261d98a9e5d5f8292720f73ea2ac92915eaa1e` |

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
| L1 | ±1 IRE | confirmed (+0.06) | confirmed (+0.06) | confirmed (+0.06) | confirmed (+0.06) | confirmed (−0.00) | confirmed (+0.01) |
| L2 | ±1 IRE | confirmed (+0.04) | confirmed (+0.05) | confirmed (+0.04) | confirmed (+0.05) | confirmed (−0.00) | confirmed (−0.00) |
| L3b | ±1 IRE | confirmed (+0.04) | confirmed (+0.06) | confirmed (+0.05) | confirmed (+0.06) | confirmed (−0.00) | confirmed (+0.00) |
| L3l | ±1 IRE | confirmed (+0.05) | confirmed (+0.06) | confirmed (+0.06) | confirmed (+0.06) | confirmed (+0.00) | confirmed (+0.00) |
| L4 | ±1 IRE | confirmed (+0.05) | confirmed (+0.07) | confirmed (+0.06) | confirmed (+0.07) | confirmed (+0.00) | confirmed (+0.00) |
| L5 | ±1 IRE | confirmed (+0.07) | confirmed (+0.07) | confirmed (+0.07) | confirmed (+0.07) | confirmed (+0.01) | confirmed (−0.00) |
| C1 | ±2 % | refuted (+4.45) | confirmed (−1.24) | confirmed (+0.68) | refuted (−3.84) | confirmed (+0.16) | refuted (−15.06) |
| C2 | ±1 deg | refuted (+2.22) | refuted (+1.02) | confirmed (+0.58) | refuted (+1.11) | confirmed (+0.05) | refuted (−3.06) |
| C3a | ±2 % | refuted (+3.91) | refuted (+0.27) | refuted (+1.20) | refuted (−1.43) | confirmed (+0.12) | refuted (−15.22) |
| C3p | ±1 deg | refuted (+1.52) | refuted (+0.94) | refuted (+0.61) | refuted (+1.01) | confirmed (+0.07) | refuted (−1.92) |
| F1 0.5 | ±0.5 dB | confirmed (+0.09) | confirmed (+0.02) | confirmed (+0.02) | confirmed (+0.02) | confirmed (+0.00) | confirmed (−0.07) |
| F1 1 | ±0.5 dB | confirmed (+0.16) | confirmed (−0.07) | confirmed (−0.07) | confirmed (−0.07) | confirmed (+0.01) | confirmed (−0.27) |
| F1 2 | ±0.5 dB | confirmed (+0.12) | confirmed (−0.18) | confirmed (−0.20) | confirmed (−0.18) | confirmed (+0.01) | refuted (−0.90) |
| F1 3 | ±0.5 dB | confirmed (+0.47) | confirmed (−0.22) | confirmed (−0.31) | confirmed (−0.27) | confirmed (+0.01) | refuted (−1.45) |
| F1 3.58 | ±0.5 dB | confirmed (+0.39) | confirmed (−0.05) | confirmed (+0.02) | confirmed (−0.32) | confirmed (+0.03) | refuted (−1.49) |
| F1 4.2 | reported | −1.05 | −0.63 | −0.43 | −0.65 | +0.04 | −1.21 |
| F2a | reported | +5.37 | +5.55 | +2.86 | +4.81 | +4.70 | −3.01 |
| F2d | reported | −10.01 | −16.18 | −9.41 | −14.91 | −12.18 | −2.34 |
| F3g | ±2 % | refuted (+2.35) | confirmed (−0.70) | confirmed (−1.23) | refuted (−2.97) | confirmed (−0.04) | refuted (−15.74) |
| F3d | ±15 ns | refuted (+35.46) | refuted (+12.52) | refuted (+17.62) | refuted (+13.36) | confirmed (+2.78) | refuted (−16.41) |
| T1 | reported | −7.74 | −4.32 | −5.25 | −4.31 | +0.57 | +59.42 |
| T2 | reported | −9.28 | −4.97 | −6.12 | −4.91 | −1.62 | +1.82 |
| V1 | ±0 lines | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) |
| V2 | ±0 lines | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) |
| V3 | ±0 lines | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) | confirmed (0) |

ld-decode recovers the levels (L1–L5) and the VBI codes (V1–V3) on every
baseline: every mean within 0.08 IRE, every frame within 0.3 IRE, and every
code exact. On the CAV disc baselines its chroma level and response (C1,
F1 3.58 MHz, F3g) are confirmed. They are refuted on the carrier diagnostic
and, for C1 and F3g, on CLV. Chroma phase (C2, C3p), the staircase gain
spread (C3a) and the chroma-to-luma delay (F3d) are refuted on all four
baselines except C2 at 145 mm. The reference decoder
confirms every judged measurement on the carrier diagnostic, so the FM
signal carries the truth by the standard. 0H (T1) and SC/H (T2) are
reported.

#### Carrier diagnostic

ld-decode command: `ld-decode --NTSC -m 0 carrier.s16 carrier_decoded`. Verdict: **refuted**.

| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.030 | +0.055 | +0.134 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | +0.005 | +0.036 | +0.069 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | +0.033 | +0.044 | +0.055 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | +0.040 | +0.052 | +0.066 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | +0.042 | +0.054 | +0.068 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | +0.000 | +0.072 | +0.134 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | +4.358 | +4.448 | +4.552 | 49 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | +1.985 | +2.216 | +2.388 | 49 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | +2.488 | +3.911 | +5.260 | 49 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -0.054 | +1.517 | +2.408 | 49 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | +0.081 | +0.091 | +0.099 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | +0.147 | +0.157 | +0.167 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | +0.108 | +0.120 | +0.132 | 0 | confirmed |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | +0.459 | +0.475 | +0.498 | 0 | confirmed |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | +0.367 | +0.386 | +0.405 | 0 | confirmed |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | -1.067 | -1.051 | -1.030 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | +5.292 | +5.367 | +5.500 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | -10.375 | -10.006 | -9.699 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | +2.107 | +2.351 | +2.496 | 49 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | +34.197 | +35.462 | +37.188 | 49 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | -8.553 | -7.737 | -6.785 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -10.318 | -9.280 | -8.430 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.090 | +1.649 | +4.831 | — | reported |
| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.074 | -0.001 | +0.074 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | -0.030 | -0.002 | +0.025 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | -0.009 | -0.001 | +0.007 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | -0.006 | +0.001 | +0.008 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | -0.005 | +0.000 | +0.006 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | -0.022 | +0.011 | +0.051 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | +0.048 | +0.157 | +0.256 | 0 | confirmed |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | -0.208 | +0.050 | +0.221 | 0 | confirmed |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | -0.447 | +0.125 | +0.702 | 0 | confirmed |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -0.316 | +0.069 | +0.459 | 0 | confirmed |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | -0.006 | +0.001 | +0.010 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.003 | +0.007 | +0.020 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.007 | +0.005 | +0.017 | 0 | confirmed |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -0.011 | +0.008 | +0.031 | 0 | confirmed |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | +0.012 | +0.030 | +0.048 | 0 | confirmed |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | +0.022 | +0.036 | +0.061 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | +4.637 | +4.695 | +4.844 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | -12.606 | -12.183 | -11.866 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | -0.247 | -0.041 | +0.108 | 0 | confirmed |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | +1.227 | +2.778 | +4.055 | 0 | confirmed |
| T1 | 0H position difference, all normal-sync lines | ns | reported | +0.026 | +0.570 | +1.172 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -2.482 | -1.618 | -0.737 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.053 | +0.803 | +2.151 | — | reported |

Discontinuities: lead-in frames 0-1 not decoded (before the first decoded frame); pictures 50-50 not decoded (after the last decoded frame).

#### Ideal disc, CAV, 55 mm

ld-decode command: `ld-decode --NTSC cav_inner.s16 cav_inner_decoded`. Verdict: **refuted**.

| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.089 | +0.058 | +0.193 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | +0.005 | +0.046 | +0.079 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | +0.046 | +0.058 | +0.072 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | +0.053 | +0.063 | +0.074 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | +0.055 | +0.067 | +0.079 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | +0.025 | +0.074 | +0.121 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | -1.593 | -1.242 | -0.924 | 0 | confirmed |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | +0.648 | +1.025 | +1.363 | 41 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | -1.860 | +0.271 | +3.236 | 47 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -0.592 | +0.945 | +2.195 | 49 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | +0.009 | +0.021 | +0.029 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.079 | -0.066 | -0.053 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.212 | -0.180 | -0.157 | 0 | confirmed |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -0.249 | -0.219 | -0.182 | 0 | confirmed |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | -0.108 | -0.047 | -0.002 | 0 | confirmed |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | -0.662 | -0.633 | -0.605 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | +5.307 | +5.549 | +5.859 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | -16.787 | -16.185 | -15.643 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | -1.179 | -0.704 | -0.282 | 0 | confirmed |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | +8.691 | +12.517 | +15.159 | 1 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | -5.988 | -4.317 | -2.134 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -7.013 | -4.973 | -1.964 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.136 | +1.169 | +3.461 | — | reported |
| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.253 | +0.018 | +0.327 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | -0.084 | +0.001 | +0.238 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | -0.014 | +0.000 | +0.016 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | -0.012 | +0.001 | +0.016 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | -0.009 | +0.002 | +0.014 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | -0.156 | +0.005 | +0.185 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | -20.515 | -16.458 | -12.529 | 50 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | -5.805 | -2.840 | +0.199 | 50 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | -59.852 | -24.435 | -5.802 | 50 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -7.028 | -0.417 | +7.473 | 50 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | -0.082 | -0.067 | -0.043 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.316 | -0.270 | -0.232 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.976 | -0.913 | -0.854 | 50 | refuted |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -1.604 | -1.441 | -1.276 | 50 | refuted |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | -2.162 | -1.554 | -1.052 | 50 | refuted |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | -6.897 | -6.638 | -6.378 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | -10.121 | -8.164 | -6.102 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | +9.054 | +15.171 | +21.670 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | -21.872 | -19.344 | -16.532 | 50 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | -51.108 | -20.844 | +8.228 | 33 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | +47.824 | +59.177 | +66.899 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -12.773 | +1.140 | +11.288 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.631 | +12.166 | +27.154 | — | reported |

Discontinuities: lead-in frames 0-1 not decoded (before the first decoded frame); pictures 50-50 not decoded (after the last decoded frame).

#### Ideal disc, CAV, 144.83–145 mm

ld-decode command: `ld-decode --NTSC cav_outer.s16 cav_outer_decoded`. Verdict: **refuted**.

| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.060 | +0.061 | +0.164 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | +0.005 | +0.042 | +0.079 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | +0.040 | +0.051 | +0.063 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | +0.048 | +0.060 | +0.070 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | +0.051 | +0.064 | +0.076 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | +0.022 | +0.073 | +0.123 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | +0.520 | +0.676 | +0.797 | 0 | confirmed |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | +0.353 | +0.579 | +0.750 | 0 | confirmed |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | -0.433 | +1.202 | +3.251 | 49 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -0.846 | +0.611 | +1.605 | 48 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | +0.016 | +0.023 | +0.032 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.079 | -0.067 | -0.057 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.210 | -0.197 | -0.182 | 0 | confirmed |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -0.331 | -0.313 | -0.295 | 0 | confirmed |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | +0.002 | +0.019 | +0.038 | 0 | confirmed |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | -0.446 | -0.426 | -0.396 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | +2.628 | +2.857 | +3.008 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | -9.920 | -9.412 | -8.757 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | -1.597 | -1.231 | -0.716 | 0 | confirmed |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | +16.182 | +17.618 | +19.848 | 49 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | -6.204 | -5.253 | -4.308 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -7.332 | -6.123 | -5.006 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.098 | +1.201 | +3.766 | — | reported |
| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.074 | +0.006 | +0.089 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | -0.035 | -0.002 | +0.030 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | -0.008 | +0.000 | +0.008 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | -0.007 | +0.001 | +0.011 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | -0.007 | +0.000 | +0.008 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | -0.056 | -0.004 | +0.045 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | -15.173 | -15.061 | -14.927 | 50 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | -3.319 | -3.062 | -2.819 | 50 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | -16.578 | -15.222 | -12.650 | 50 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -4.491 | -1.923 | +0.255 | 50 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | -0.075 | -0.069 | -0.059 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.283 | -0.270 | -0.255 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.919 | -0.895 | -0.878 | 50 | refuted |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -1.473 | -1.452 | -1.422 | 50 | refuted |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | -1.519 | -1.494 | -1.461 | 50 | refuted |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | -1.236 | -1.210 | -1.178 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | -3.122 | -3.009 | -2.892 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | -2.873 | -2.344 | -1.873 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | -16.223 | -15.739 | -15.188 | 50 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | -18.375 | -16.413 | -14.116 | 45 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | +58.772 | +59.424 | +60.212 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | +0.840 | +1.822 | +2.880 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.064 | +12.533 | +27.382 | — | reported |

Discontinuities: lead-in frames 0-1 not decoded (before the first decoded frame); pictures 50-50 not decoded (after the last decoded frame).

#### Ideal disc, CLV, 10.1 m/s from 55 mm

ld-decode command: `ld-decode --NTSC clv.s16 clv_decoded`. Verdict: **refuted**.

| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.089 | +0.058 | +0.298 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | -0.010 | +0.047 | +0.084 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | +0.048 | +0.059 | +0.070 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | +0.041 | +0.063 | +0.080 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | +0.056 | +0.068 | +0.079 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | +0.022 | +0.075 | +0.145 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | -3.946 | -3.844 | -3.745 | 49 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | +0.528 | +1.115 | +1.545 | 46 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | -5.516 | -1.426 | +3.772 | 49 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -0.531 | +1.010 | +2.678 | 49 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | -0.000 | +0.021 | +0.030 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.079 | -0.066 | -0.052 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.213 | -0.182 | -0.152 | 0 | confirmed |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -0.337 | -0.267 | -0.193 | 0 | confirmed |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | -0.353 | -0.316 | -0.277 | 0 | confirmed |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | -0.680 | -0.653 | -0.633 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | +4.431 | +4.813 | +5.162 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | -15.429 | -14.913 | -14.459 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | -3.380 | -2.972 | -2.599 | 49 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | +10.286 | +13.361 | +16.111 | 7 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | -8.655 | -4.308 | -2.675 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -10.376 | -4.914 | -2.841 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.163 | +1.244 | +3.488 | — | reported |
| # | Measurement | Unit | Threshold | Min | Mean | Max | Frames outside | Verdict |
|---|-------------|------|-----------|-----|------|-----|----------------|---------|
| L1 | Blanking level (back porch), all normal-sync lines | IRE | ±1 | -0.268 | +0.019 | +0.298 | 0 | confirmed |
| L2 | Sync tip level, all normal-sync lines | IRE | ±1 | -0.089 | -0.002 | +0.213 | 0 | confirmed |
| L3b | VIRS black reference, lines 19/282 | IRE | ±1 | -0.016 | -0.000 | +0.012 | 0 | confirmed |
| L3l | VIRS luminance reference, lines 19/282 | IRE | ±1 | -0.015 | +0.001 | +0.016 | 0 | confirmed |
| L4 | NTC-7 white bar, line 20 | IRE | ±1 | -0.006 | +0.003 | +0.016 | 0 | confirmed |
| L5 | NTC-7 staircase treads, line 20 | IRE | ±1 | -0.141 | +0.001 | +0.140 | 0 | confirmed |
| C1 | VIRS chroma amplitude, decoded/truth − 1 | % | ±2 | -54.740 | -54.557 | -54.427 | 50 | refuted |
| C2 | VIRS chroma phase relative to burst | deg | ±1 | +0.203 | +3.454 | +6.147 | 50 | refuted |
| C3a | Staircase chroma amplitude (DG), decoded/truth − 1 | % | ±2 | -54.467 | -34.340 | -11.014 | 50 | refuted |
| C3p | Staircase chroma phase (DP) relative to burst | deg | ±1 | -10.328 | +0.881 | +12.585 | 50 | refuted |
| F1 0.5 | Multiburst 0.5 MHz, line 283 | dB | ±0.5 | -0.092 | -0.073 | -0.049 | 0 | confirmed |
| F1 1 | Multiburst 1 MHz, line 283 | dB | ±0.5 | -0.300 | -0.270 | -0.241 | 0 | confirmed |
| F1 2 | Multiburst 2 MHz, line 283 | dB | ±0.5 | -0.987 | -0.924 | -0.872 | 50 | refuted |
| F1 3 | Multiburst 3 MHz, line 283 | dB | ±0.5 | -1.777 | -1.479 | -1.184 | 50 | refuted |
| F1 3.58 | Multiburst 3.58 MHz, line 283 | dB | ±0.5 | -5.379 | -4.772 | -4.250 | 50 | refuted |
| F1 4.2 | Multiburst 4.2 MHz, line 283 | dB | reported | -7.066 | -6.737 | -6.465 | — | reported |
| F2a | 2T pulse amplitude, decoded/truth − 1 | % | reported | -12.913 | -10.601 | -8.268 | — | reported |
| F2d | 2T pulse half-amplitude duration difference | ns | reported | +16.497 | +23.068 | +29.411 | — | reported |
| F3g | 12.5T chroma-to-luma gain, decoded/truth − 1 | % | ±2 | -39.511 | -37.177 | -34.614 | 50 | refuted |
| F3d | 12.5T chroma-to-luma delay difference | ns | ±15 | -84.762 | -41.112 | -12.629 | 49 | refuted |
| T1 | 0H position difference, all normal-sync lines | ns | reported | +47.991 | +58.953 | +66.804 | — | reported |
| T2 | Burst phase at 0H (SC/H) difference | deg | reported | -14.302 | +0.903 | +12.626 | — | reported |
| V1 | FM codes, lines 10/273: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V2 | White flags, lines 11/274: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| V3 | Biphase codes, lines 16-18/279-281: lines wrong | lines | ±0 | 0 | 0 | 0 | 0 | confirmed |
| P1 | Active picture RMS difference | IRE | reported | +0.541 | +11.385 | +25.061 | — | reported |

Discontinuities: lead-in frames 0-1 not decoded (before the first decoded frame); pictures 50-50 not decoded (after the last decoded frame).

### Triage (plan Task 7.5)

Every refuted prediction is classified below. The evidence, and what would
settle each unattributed discrepancy, are in
[ld-decode-findings.md](ld-decode-findings.md).

| Refuted | Baselines | Class | Evidence |
|---------|-----------|-------|----------|
| C1, C3a, F1 3.58 MHz, F3g (chroma level and response), before the pin update | disc baselines | Measurement (corrected) | The truth's colour burst lasted 7.25 cycles from 5.63 µs, short of the 9 ± 1 cycles from 5.3 µs that IEC 60857 §9.1.2 and BT.470-6 require (videosynth#32). ld-decode measured it low and over-corrected chroma (C1 +9.5, +11.3 and +6.9 %). videosynth `cd9e698` renders the standard burst (5.34 µs, 9.0 cycles), and C1, F1 3.58 MHz and F3g now confirm at 55 and 145 mm (ld-decode-findings.md, "The colour burst") |
| C1, F3g (chroma level) | CLV | Unattributed (U1) | C1 −3.8 %, F3g −3.0 %; F1 3.58 MHz confirms. At 55 mm on CLV the upper chroma sidebands lie near the optical cut-off, where the RF cannot be checked against the truth (ld-decode-findings.md, reason 2) |
| C1, C3a, F3g (chroma level) | carrier only | Unattributed (U1) | The reference decoder recovers chroma on the carrier diagnostic within 0.2 %, so the FM signal is right. ld-decode reads C1 +4.4 % there with its burst-driven correction (−9.3 % without). ld-decode is not built for MTF-free RF |
| F3d (chroma-to-luma delay) | all four (ld-decode) | Unattributed (U2) | The reference decoder with the §9.1.7 playback equaliser reads +2.8 ns on the carrier diagnostic; ld-decode adds about 116 ns against the 80 ns the pre-distortion presumes. At 55 mm the mean is +12.5 ns, with one frame outside. ld-decode shifts six real discs' timing by the same +34 to +42 ns as ld-creator's RF, but the discs fall into two groups about 30 ns apart, and their own VITS timing is unknown |
| C2, C3p, and C3a's spread (chroma phase, differential phase and gain) | all (C2: not at 145 mm) | Unattributed (U4) | The reference decoder reads +0.05° and within ±0.5° on the carrier diagnostic. Every real disc compared decodes with two to four times ld-creator's differential phase and gain, but through the IEC chain the two do not differ |
| F2a (2T amplitude) | — | Measurement (corrected) | The truth's 2T is not band-limited, so any band-limited decoder misses ±2 % (the reference decoder reads +4.7 %); F2a is now reported, not judged |
| Every chroma row of the MTF bisection run | `cav_inner_equalised` (diagnostic) | Measurement (the run's design) | At 55 mm the S12 equaliser tapers to zero over 10.9–13.09 MHz, where the upper chroma sideband lies |

No discrepancy is a model finding: S2–S5 decode to the truth through the
reference decoder, and S6–S13 do too at 145 mm with the `ideal` capture.
None is a lead-in shortfall: no failure is confined to the first programme
frames. None is a decoder finding:
ld-decode is calibrated on real discs, and the real captures compared (six
discs, one player) differ from each other by about 3 dB in recorded chroma
and 30 ns in chroma timing. None of the discs' own VITS are known, so they
show where ld-creator's RF sits among real discs, not which is right (see
"Triage classes" and ld-decode-findings.md, "The real capture" and "Other
discs").

## Triage classes

| Class | Meaning |
|-------|---------|
| Model | A stage does not conform to its note. Fix the stage, then rerun |
| Lead-in shortfall | Failures are confined to the first programme frames and stop once the decoder has settled. Lengthen the lead-in and rerun |
| Decoder finding | Every stage conforms to its note, the RF is shown to carry the right signal, and real-disc evidence shows that the RF resembles a real disc where the failure arises (a real capture decoded with the same settings settles the same way, or the decoder's behaviour is wrong on real discs too). Recorded with the evidence in `ld-decode-findings.md` |
| Unattributed | Every stage conforms to its note and the RF carries the IEC 60857 signal, but the decoder disagrees, and nothing yet shows whether ld-creator's IEC-nominal disc or the decoder's calibration departs from real discs. The decoder is calibrated on real discs, not on the standard, so conformance to the standard alone does not make the decoder wrong. Held open, with the evidence and what would settle it, in `ld-decode-findings.md` |
| Measurement | The comparator, or the truth it compares against, is wrong. Fix the comparator, and check it against the truth-against-truth and +2 IRE tests (Task 7.2); a truth that departs from its standard is reported against videosynth and shown by a bisection run with the truth corrected |
