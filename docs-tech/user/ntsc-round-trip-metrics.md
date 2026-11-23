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

The videosynth project of plan Task 7.1: 100 NTSC CAV frames.

| Frames | Content | VBI |
|--------|---------|-----|
| 0–49 | Lead-in: flat grey | Lead-in code `88FFFF` on lines 17, 18, 280 and 281 (§10.1.1); FM picture number 0 (§10.2.3); white flag on line 11 (§10.2.1) |
| 50–99 | Programme: 75 % colour bars, luma ramp, multiburst, flat grey, in turn | CAV picture numbers 1–50 (§10.1.3, §10.2.3); first-field white flag (§10.2.4) |

Every frame carries VIRS on lines 19 and 282 (§9.1.3), the NTC-7 composite
signal on line 20, and the NTC-7 combination signal on line 283 (§9.1.4).

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
implemented by the pinned videosynth. The comparator's window table cites the
definition for each window.

| # | Line(s) | Measurement | Method | Threshold | Reason for the threshold |
|---|---------|-------------|--------|-----------|--------------------------|
| L1 | all | Blanking level | mean of the back porch after the burst | ±1 IRE | Plan Task 1.5. Ten times the 0.1 IRE the model's own error budget allows (S7) |
| L2 | all | Sync tip level | mean of the sync tip centre | ±1 IRE | as L1 |
| L3 | 19, 282 (VIRS) | Black reference (7.5 IRE), luminance reference (50 IRE) | window means | ±1 IRE | as L1 |
| L4 | 20 (NTC-7 composite) | White bar (100 IRE) | window mean | ±1 IRE | as L1 |
| L5 | 20 | Staircase steps | each step's mean | ±1 IRE | as L1 |
| C1 | 19, 282 | VIRS chroma reference amplitude | least-squares sinusoid at fsc; ratio decoded / truth | ±2 % | Plan Task 1.5 |
| C2 | 19, 282 | VIRS chroma phase | phase relative to the line's burst; difference | ±1° | Plan Task 1.5 |
| C3 | 20 | Staircase chroma amplitude and phase (differential gain and phase) | as C1/C2, per step | ±2 %, ±1° | as C1/C2 |
| F1 | 283 (NTC-7 combination) | Multiburst packets, 0.5–4.2 MHz | least-squares sinusoid amplitude per packet; ratio in dB | ±0.5 dB to 3.58 MHz; reported at 4.2 MHz | The disc's pre-emphasis and the decoder's de-emphasis are complementary, and the decoder compensates the readout MTF that the RF carries (see below) |
| F2 | 20 | 2T pulse | peak amplitude ratio; half-amplitude duration difference | ±2 %; reported | Amplitude as C1. Duration is reported until Task 7.4's carrier-diagnostic baseline establishes the decoder's own value |
| F3 | 20 | 12.5T pulse | chroma-to-luma gain ratio; chroma-to-luma delay difference | ±2 %; ±15 ns | Delay: the §9.1.7 tolerance at 3.58 MHz. The disc's pre-distortion is specified to ±15 ns there, so a decoder that applies the matching playback equalisation leaves a residual within it |
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
  to the optical cut-off), decoded with `-m 0`.

If that run passes and the normal run fails, the disagreement is between the
decoder's MTF compensation and S11. It is recorded as a decoder finding only
if S11 is also supported by the Figure 18 HFMI check and its sources.
Otherwise it is a model finding for Task 10.1.

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

Task 7.4 copies these tables into this document under a heading per
baseline, with the tool versions (videosynth and ld-decode flake pins,
ld-creator commit) and the resolved project files.

## Triage classes

| Class | Meaning |
|-------|---------|
| Model | A stage does not conform to its note. Fix the stage, then rerun |
| Lead-in shortfall | Failures are confined to the first programme frames and stop once the decoder has settled. Lengthen the lead-in and rerun |
| Decoder finding | Every stage conforms to its note, and the carrier diagnostic either reproduces the failure or bisects it to a stage that conforms. Recorded with the evidence in `ld-decode-findings.md` (created by plan Task 7.5) |
| Measurement | The comparator is wrong. Fix it, and check it against the truth-against-truth and +2 IRE tests (Task 7.2) |
