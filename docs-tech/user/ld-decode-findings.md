# ld-decode findings

The discrepancies the NTSC round trip has found, their evidence, and how
they are attributed (plan Task 7.5). The measurements, thresholds and triage
classes are defined in [ntsc-round-trip-metrics.md](ntsc-round-trip-metrics.md),
and the baseline tables are in its "Results" section.

**No ld-decode finding is established.** The largest discrepancy,
ld-decode's high chroma level on disc RF, came from the round trip's truth:
videosynth's colour burst was shorter than the standard allows
([The colour burst](#the-colour-burst)). videosynth now renders the
standard burst, and with it ld-decode's chroma level and response are
confirmed on both CAV disc baselines. The remaining discrepancies are
**unattributed**. ld-creator's RF carries the IEC 60857 signal, and ld-decode
disagrees with the truth, but ld-decode is calibrated on real discs, not on
the standard. One real capture has been compared in detail
([The real capture](#the-real-capture)), and seven more for the same
quantities at one radius ([Other discs](#other-discs)). The eight captures
span six discs, all from one player. They show that real discs differ
from each other by about 3 dB in recorded chroma and by about 30 ns in
chroma timing. ld-creator's IEC-nominal disc lies at the high-chroma end
of that range, nearest GGV1069. None of these discs' own VITS are known, so
they show where ld-creator's RF sits among real discs, not which of them is
right.

Everything about ld-decode is taken from its external interface: its command
line, its CVBS output, and the capture notes and log it writes. ld-decode's
source was not read.

## Versions

| Tool or source | Version |
|----------------|---------|
| ld-decode | `8.0.0pre1+git.d6055b3` (flake pin `d6055b303f67869d005a23619e886453ff73130d`) |
| videosynth | `cd9e698` (flake pin `cd9e698b08965f0d6dd2195c456e121f1dbdc53b`) |
| ld-creator | `cea9361` plus the working tree |
| Real capture | Pioneer GGV1069 (NTSC CAV test disc), side 1, DomesdayDuplicator capture of 2019, 21.6 GB `.ldf`; not in the repository |
| Other captures | Seven DomesdayDuplicator captures of NTSC discs, all on the same Pioneer LD-V4300D as the GGV1069 capture ([Other discs](#other-discs)); not in the repository |

The videosynth pin includes the fixes for three truth defects the round trip
found:
- the short colour burst
  ([videosynth#32](https://github.com/decode-orc/videosynth/issues/32));
- VIRS levels and layout that departed from IEC 60857 Figure 6b, and no FM
  picture number in the lead-in
  ([videosynth#31](https://github.com/decode-orc/videosynth/issues/31));
- VITS chroma whose phase flipped 180° from line to line
  ([videosynth#33](https://github.com/decode-orc/videosynth/issues/33)).

## What is established

### The RF carries the IEC 60857 signal

[`scripts/reference_demod.py`](../../scripts/README.md) is a demodulator
written from IEC 60857 alone. It shares no code with ld-creator or
ld-decode. Its chain:

- analytic-signal FM demodulation;
- the §9.2.1–§9.2.3 carrier frequencies;
- the exact inverse of the §9.2.4 pre-emphasis;
- the playback group delay that §9.1.7's pre-distortion is designed for;
- for disc RF, division by the readout transfer, computed independently by
  quadrature.

Its self-tests recover synthetic FM to 0.1 %. Its decodes of ld-creator's
RF, scored by the same comparator:

| RF | Levels | C1 | C2 | C3a | F1 3.58 MHz | F1 4.2 MHz | F3g | F3d | Verdict |
|----|--------|----|----|-----|-------------|------------|-----|-----|---------|
| Carrier diagnostic | ±0.08 IRE | +0.2 % | +0.1° | −0.4 to +0.7 % | +0.03 dB | +0.04 dB | −0.0 % | +2.8 ns | confirmed |
| Carrier diagnostic, no playback equaliser | ±0.17 IRE | +0.1 % | +0.1° | −2.7 to +1.0 % | +0.03 dB | −0.02 dB | −0.1 % | −82.2 ns | C3a, C3p, F3d refuted |
| CAV 145 mm, `ideal` capture | ±0.05 IRE | +0.5 % | +0.1° | −0.2 to +0.8 % | +0.03 dB | +0.03 dB | −0.0 % | +2.7 ns | confirmed |
| CAV 145 mm, DdD capture (not compensated) | ±0.09 IRE | −15.1 % | −3.1° | −16.6 to −12.7 % | −1.49 dB | −1.21 dB | −15.7 % | −16.4 ns | refuted |

So:

- **S2–S5 are right end to end.** The carrier diagnostic decodes to the
  truth within every judged threshold. With no playback equaliser, chroma
  arrives 82 ns early: that is S3's §9.1.7 pre-distortion, which a player is
  meant to cancel. The staircase chroma, which is short, then also misses
  its treads on some frames.
- **S6–S13 are right where the readout does not intervene.** At 145 mm the
  optical cut-off is 34.5 MHz, far above every sideband. There the disc RF
  with the `ideal` capture decodes to the truth as well.
- **The DdD capture filter alone moves chroma by about 15 %.** Any decoder
  of DdD captures has to compensate that response, and ld-decode's was set
  on real DdD captures. The filter is fitted to a real board's measured
  response, within 0.17 dB and 6° (S13 note, "The fit").

### Levels and VBI codes

ld-decode recovers every level and every VBI code on all four baselines.
Every level mean is within 0.08 IRE and every frame within 0.3 IRE, and
every code is exact, including the FM picture number 0 that the lead-in now
carries.

### Chroma level on CAV disc RF

With the standard burst, ld-decode's chroma level and response are confirmed
at 55 mm and at 145 mm:

| Baseline | C1 | F1 3.58 MHz | F3g |
|----------|----|-------------|-----|
| CAV 55 mm | −1.2 % | −0.05 dB | −0.7 % |
| CAV 145 mm | +0.7 % | +0.02 dB | −1.2 % |

### Errors on the round trip's side, corrected or attributed

- **The colour burst.** The truth's burst was about 7.3 cycles long, and
  ld-decode's chroma correction read it low and over-corrected. It has been
  corrected in videosynth. See the next section.
- **F2a, 2T pulse amplitude.** The truth's 2T pulse is not band-limited.
  videosynth samples the 250 ns sin² pulse directly at 4fsc, so it carries
  energy up to 7 MHz. Any decoder that limits the video band changes the
  pulse's peak by several percent. The reference decoder, flat to 4.6 MHz,
  reads it +4.7 % high, and ld-decode, with its chroma correction off, +2.7 %.
  The ±2 % threshold could not be met by any correct decoder, so F2a is now
  reported, not judged (Measurement class).
- **The MTF bisection run** (S12 equaliser, decoded with `-m 0`) cannot test
  chroma at 55 mm. The equaliser tapers to zero between 0.833·fc and fc,
  10.9 to 13.09 MHz, where the upper chroma sideband lies (Measurement class;
  metrics document, "Readout MTF and decoder settings").

## The colour burst

IEC 60857 §9.1.2 calls for the "standard EIA recommended burst" (EIA
Television Systems Bulletin No. 4). ITU-R BT.470-6 Table 2 items 2.14g/h
give it for M/NTSC: it starts 5.3 µs after 0H (4.71–5.71 µs) and lasts
9 ± 1 cycles (2.23–3.11 µs), both at the envelope's 50 % points.
[`vits_measure.py`](../../scripts/README.md#real-captures) measures:

| Source | Burst start | Duration |
|--------|-------------|----------|
| Truth, videosynth `a995726` (before the fix) | 5.63 µs | 7.25 cycles |
| Truth, videosynth `cd9e698` (pinned) | 5.34–5.35 µs | 9.0 cycles |
| GGV1069, ld-decode's decode, all three excerpts | 5.33–5.35 µs | 8.91 cycles |
| BT.470-6 | 5.3 µs | 9 cycles (8–10) |

The earlier videosynth's burst gate was 5.3–7.97 µs, but its envelope's
ramps lay inside the gate, so the 50 % points fell inside it too
([videosynth#32](https://github.com/decode-orc/videosynth/issues/32)).

ld-decode sets its inverse-MTF chroma strength from the burst it measures.
On the short burst it measured too little burst. On the carrier diagnostic
its log reported the uncorrected burst at 15.65 IRE, where its own
uncorrected output (`--no_auto_chroma`) carried 17.4 IRE. With the standard
burst the two are 17.4 and 17.9 IRE. On the real disc they agree (17.3 and
17.2 IRE at the inner excerpt).

**Bisection, then the fix.** Before the fix,
[`standard_burst.py`](../../scripts/README.md#real-captures) copied the
truth with every burst replaced by the BT.470-6 burst, and nothing else
changed. That moved C1 on the disc baselines from +9.5, +11.3 and +6.9 % to
+0.7, +2.5 and −2.2 %. The fixed videosynth renders that burst itself, and
the round trip on its truth gives:

| Baseline | ld-decode's burst estimate | Chroma strength | C1 | C2 | C3a | F1 3.58 MHz | F3g | F3d |
|----------|----------------------------|-----------------|----|----|-----|-------------|-----|-----|
| Carrier diagnostic (`-m 0`) | 17.4 IRE | 0.735 | +4.4 % | +2.2° | +2.5 to +5.3 % | **+0.39 dB** | +2.4 % | +35.5 ns |
| CAV 55 mm | 17.5 IRE | 0.689 | **−1.2 %** | +1.0° | −1.9 to +3.2 % | **−0.05 dB** | **−0.7 %** | **+12.5 ns** (1 frame outside) |
| CAV 145 mm | 16.9 IRE | 0.878 | **+0.7 %** | **+0.6°** | −0.4 to +3.3 % | **+0.02 dB** | **−1.2 %** | +17.6 ns |
| CLV | 17.9, then 19.3 IRE | 0.764 | −3.8 % | +1.1° | −5.5 to +3.8 % | **−0.32 dB** | −3.0 % | +13.4 ns (7 frames outside) |

Bold values are within their thresholds (F3d by its mean). C1 differs
from the bisection's by 1.6–1.9 %. Unlike the bisection, the fixed truth
also changes the VIRS (a 70 IRE pedestal, 12–36 µs) and the VITS chroma
phase ([videosynth#31](https://github.com/decode-orc/videosynth/issues/31),
[#33](https://github.com/decode-orc/videosynth/issues/33)), and that
difference is not separated further.

ld-decode's estimate also depends on the burst's edges. A bisection copy
with faster edges (180 ns, 10–90 %) gave 17.3 IRE and chroma strength 0.752
at 55 mm, so its burst measurement integrates over a window that the
burst's edges reach into.

ld-decode's calibration at 55 mm is now close to the real disc's at its
inner radius (CAV frames 18–57): there it measures the uncorrected burst at
17.3 IRE and sets chroma strength 0.751.

**Class: Measurement** (the truth was wrong; reported and fixed in
videosynth). The pinned videosynth carries the fix, and the recorded results
use it.

## The real capture

### Excerpts

Cut from the capture with `ld-cut` (40 frames each, raw 16-bit RF at
40 MSPS, into the build directory):

| Excerpt | File frames | CAV frames | Radius (approximate) |
|---------|-------------|------------|----------------------|
| Inner | 300–339 | 18–57 | 55 mm |
| Middle | 12000–12039 | 11715–11754 | 74.6 mm |
| Outer | 25000–25039 | 24715–24754 | 96.3 mm |

The side ends near file frame 25,600. The radius is 55 mm plus 1.6667 µm
per frame, the round-trip projects' pitch. ld-creator RF made at those radii
(the round-trip CAV project with only `start_radius_mm` changed) matches
the excerpt's carrier envelope slope, blanking (8.1 MHz) against sync tip
(7.6 MHz), within 0.03: 0.856 against 0.882, 0.936 against 0.939, and
0.969 against 0.963. So the radii match.

GGV1069 carries the NTC-7 composite on line 13, the NTC-7 combination on
line 276 and VIRS on lines 19 and 282, with 7.5 IRE setup. Its VITS chroma
is locked to the burst (+2° to +5°). The truth's now is too, and its VIRS
has the same setup: ld-decode's decodes read the VIRS black reference at
7.29–7.36 IRE on GGV1069 and 7.55 IRE on ld-creator's RF.

### ld-decode's calibration

| RF | Uncorrected burst (log) | MTF level | Chroma strength |
|----|-------------------------|-----------|-----------------|
| GGV1069 inner | 17.3 IRE | 0.987 | 0.751 |
| GGV1069 middle | 18.8 IRE | 0.776 | 0.314 |
| GGV1069 outer | 18.9 IRE | 0.654 | 0.290 |
| ld-creator 55 mm | 17.5 IRE | 1.000 | 0.689 |
| ld-creator 74.6 mm | 17.3 IRE | 0.768 | 0.765 |
| ld-creator 96.3 mm | 17.1 IRE | 0.618 | 0.829 |

ld-decode's MTF level follows the radius on both alike. At 74.6 and
96.3 mm GGV1069's burst arrives 1.5–1.8 IRE stronger than ld-creator's, and
ld-decode sets a much weaker chroma correction for it. That matches the
chroma-band difference below.

### Through the IEC chain, without compensation

`vits_measure.py rf` demodulates the RF by IEC 60857 alone, with the
playback equaliser but no readout or capture compensation, so the readout
and capture chain stays in the result. ld-creator RF and GGV1069 at the
same radius:

| Quantity | 55 mm: ld-creator / GGV1069 | 74.6 mm | 96.3 mm |
|----------|-----------------------------|---------|---------|
| Burst (IRE p-p) | 51.0 / 48.9 | 37.5 / 41.9 | 34.9 / 39.7 |
| Multiburst 3.58 MHz re 1 MHz | +2.69 / +2.41 dB | −0.33 / +0.87 dB | −1.02 / +0.51 dB |
| Multiburst 4.2 MHz re 1 MHz | +3.81 / +3.60 dB | +0.11 / +1.60 dB | −0.74 / +1.23 dB |
| 12.5T chroma / luma | 1.39 / 1.38 | 0.94 / 1.09 | 0.86 / 1.02 |
| 12.5T chroma − luma delay | −24.9 / −12.0 ± 7 ns | −23.2 / −20.8 ± 8 ns | −20.8 / −21.1 ± 5 ns |
| Differential phase, max − min | 9.9° / 6.9° | 7.9° / 8.9° | 6.7° / 9.2° |

At 55 mm the two agree within 0.3 dB. At 74.6 and 96.3 mm GGV1069 carries
1.2–1.5 dB more chroma than ld-creator. A fit of the FM sidebands
(`--sidebands`) locates the difference. The difference between the lower
and upper sideband of a packet measures the chain's tilt, not the
modulation depth. It agrees within about ±0.4 dB at 74.6 mm, and at
96.3 mm GGV1069's chain is if anything steeper. The sidebands' sum, relative
to the carrier, is larger on GGV1069 by an amount that grows with the video
frequency and is the same at both radii: −0.3 dB at 1 MHz, +0.6 dB at 2 MHz,
+1.4 dB at 3 MHz and +1.2 to +1.8 dB at 3.58 and 4.2 MHz. So either
GGV1069's chroma band is recorded with more FM deviation than ld-creator's
IEC-nominal §9.2.4 pre-emphasis gives, or its readout's response differs in
curvature over 4.5–13 MHz. The second sidebands, which would separate the
two, are too strongly shaped by the chain's tilt to do so. This is one
disc's mastering, so it is recorded as an observation, not a model defect.
The sideband figures were fitted on RF made from the earlier truth. On the
current truth, ld-creator's first sidebands differ from that fit by
0.06 dB or less, except the 3.58 MHz packet's upper sideband (+0.43 dB at
74.6 mm, +0.26 dB at 96.3 mm). That sideband lies about 14 dB below the
lower one, so the sums above are unchanged within their precision.

With the defaults, ld-decode decodes both to nominal chroma at all three
radii, so its correction absorbs the difference:

| Decode (defaults) | 55 mm: ld-creator / GGV1069 | 74.6 mm | 96.3 mm |
|-------------------|-----------------------------|---------|---------|
| Staircase chroma (IRE p-p, nominal 40) | 40.2 / 40.5 | 40.0 / 42.3 | 40.3 / 42.6 |
| VIRS chroma (IRE p-p, nominal 40) | 39.7 / 40.2 | 39.8 / 41.5 | 40.0 / 42.0 |

## Other discs

Seven more NTSC captures show how far real discs differ from each other.
All were made on the same player as the GGV1069 capture, a Pioneer
LD-V4300D, so the differences between them come from the discs, not the
player.

`scripts/capture_survey.py` cut 40 frames from file frame 1000 of each,
near the inner radius, and measured them as above. It measured each excerpt
three ways: through the IEC chain, ld-decode's decode with its defaults,
and the decode with `--no_auto_chroma`. GGV1069 appears three times: the
2019 capture and two later captures of the same disc, from 2020 and 2025.
The 2019 column here is file frame 1000, not the inner excerpt above.
ld-creator's column is the 55 mm round-trip RF.

| Quantity | ld-creator 55 mm | GGV1069 2019 | GGV1069 2020 | GGV1069 2025 | Checking Dolby Surround (CAV) | National Gallery of Art (CAV) | Computer Graphics Special (CAV, NTSC-J) | Bambi (CLV) | Cinderella (CLV) |
|---|---|---|---|---|---|---|---|---|---|
| RF envelope, blanking / sync tip | 0.86 | 0.89 | 0.89 | 0.89 | 0.90 | 0.89 | 0.90 | 0.89 | 0.90 |
| ld-decode chroma strength | 0.689 | 0.609 | 0.522 | 0.600 | 1.821 | 1.844 | 1.025 | 0.696 | 1.023 |
| ld-decode burst residual, rms | 0.22° | 0.15° | 0.14° | 0.19° | 1.86° | 4.20° (unlocked) | 0.66° | 0.24° | 0.30° |
| **IEC chain:** burst (IRE p-p) | 51.0 | 47.9 | 48.5 | 48.3 | 36.6 | 39.4 | 45.7 | 47.9 | 45.9 |
| VIRS chroma (IRE p-p) | 58.0 | 52.7 | 53.4 | 53.5 | 37.9 | 39.3 | 52.3 | 51.6 | 48.9 |
| Multiburst 2 MHz re 1 MHz | +0.58 dB | +0.35 | +0.33 | +0.39 | −1.40 | −0.14 | −3.46 | +0.34 | −0.03 |
| Multiburst 3.58 MHz re 1 MHz | +2.69 dB | +2.19 | +2.13 | +2.35 | +0.06 | +1.00 | −0.09 | +2.00 | +1.51 |
| 12.5T chroma − luma delay | −24.9 ns | −8.4 | −13.2 | −12.0 | −42.5 | −39.6 | −14.8 | −8.9 | −9.9 |
| Staircase DP, max − min | 9.9° | 6.8 | 9.3 | 7.0 | 7.7 | 9.2 | 9.9 | 8.5 | 7.8 |
| **Decode, no auto chroma:** burst (IRE p-p) | 36.1 | 36.7 | 36.9 | 36.4 | 27.5 | 29.9 | 33.3 | 35.5 | 34.6 |
| **Decode, defaults:** staircase chroma (IRE p-p, nominal 40) | 40.2 | 41.0 | 41.4 | 40.4 | 37.8 | 40.6 | 42.3 | 39.3 | 40.4 |
| VIRS chroma (IRE p-p, nominal 40) | 39.7 | 41.0 | 40.5 | 40.6 | 38.0 | 37.4 | 41.4 | 38.3 | 39.3 |
| Multiburst 2 MHz re 1 MHz | −0.11 dB | −0.04 | −0.03 | −0.10 | −1.99 | +0.54 | −4.09 | +0.19 | +0.11 |
| Multiburst 3.58 MHz re 1 MHz | +0.01 dB | +0.06 | −0.04 | +0.19 | −0.13 | +2.85 | −1.03 | −0.12 | +0.27 |
| 12.5T chroma − luma delay | +12.9 ns | +30.6 | +22.0 | +29.1 | −8.4 | +0.3 | +21.9 | +32.9 | +29.4 |
| Staircase DP, max − min | 2.7° | 5.5 | 5.6 | 5.9 | 6.7 | 9.9 | 6.1 | 5.5 | 5.3 |
| Staircase DG, max − min | 3.7 % | 7.0 | 7.5 | 8.0 | 14.0 | 16.9 | 8.5 | 11.5 | 12.3 |
| VIRS black (IRE) | 7.56 | 7.26 | 7.29 | 7.20 | 7.59 | 5.20 | 6.63 | 7.38 | 7.59 |

The full survey, with more rows, is written to `survey.md` in the
survey's output directory.

- **Captures repeat.** GGV1069's three captures, six years apart, agree
  within 0.6 IRE in burst and 0.22 dB at 3.58 MHz through the IEC chain,
  and within 1 IRE in decoded staircase chroma.
- **Recorded chroma differs between discs by about 3 dB.** Through the IEC
  chain the burst runs from 36.6 to 48.5 IRE p-p (2.4 dB) and VIRS chroma
  from 37.9 to 53.5 IRE p-p (3.0 dB). The 3.58 MHz multiburst runs from
  −0.09 to +2.35 dB.
  - GGV1069 is the real disc with the most chroma. ld-creator's IEC-nominal
    RF lies above it: 0.4–0.5 dB in burst and 0.7–0.8 dB in VIRS chroma.
  - ld-creator's RF is slightly further in, though: its envelope ratio is
    0.86 against 0.89–0.90. At the inner radius the IEC chain reads chroma
    higher (on GGV1069, the burst falls from 48.9 IRE at 55 mm to 41.9 IRE
    at 74.6 mm), so at a matched radius the gap would be smaller.
  - The two CLV discs sit just below GGV1069. The Dolby Surround and
    National Gallery of Art discs carry 2.4 and 1.8 dB less burst.
- **ld-decode absorbs the spread.** Its chroma strength runs from 0.52 to
  1.84, and it decodes every staircase to within −5.5 to +5.8 % of nominal.
  ld-creator's RF decodes at +0.5 %, inside that range, with a strength
  (0.689) inside the real discs' range. Without the correction, ld-creator's
  decoded burst (36.1 IRE) is among the GGV1069 captures' (36.4–36.9 IRE).
- **Chroma timing falls into two groups.** Through the IEC chain, five discs
  read −8 to −15 ns, and Dolby Surround and National Gallery of Art read
  −40 to −43 ns. ld-decode's decode moves every disc by +34 to +42 ns, and
  ld-creator's RF by +38 ns, so the two groups decode at +22 to +33 ns and
  −8 to 0 ns. ld-creator's −24.9 ns lies between the groups ([U2](#u2-chroma-to-luma-delay)).
- **Differential phase and gain appear in the decode.** Through the IEC
  chain, ld-creator's staircase DP (9.9°) is within the real discs' range
  (6.8–9.9°). After ld-decode's decode, every real disc shows two to four
  times ld-creator's DP and DG ([U4](#u4-chroma-phase-that-varies-with-luminance-differential-phase)).
- **Departures particular to one disc:**
  - Computer Graphics Special and Dolby Surround dip at 2 MHz, by 3.5 and
    1.4 dB through the IEC chain.
  - National Gallery of Art decoded without subcarrier lock (burst residual
    4.2° rms), with an 8.3-cycle burst and VIRS black at 5.2 IRE, so its
    phase figures are unreliable.
  - Computer Graphics Special is an NTSC-J disc, so it may carry no setup.
    Its VIRS black reads 6.6 IRE.

Each disc is one 40-frame excerpt at the inner radius, and each disc's own
VITS are unknown.

## Why the remaining discrepancies are not attributed

1. **The carrier diagnostic is not an input ld-decode is built for.** Real RF
   always carries the readout MTF, and ld-decode's MTF compensation and
   burst-driven chroma correction presumably work as a system on real RF.
   Its behaviour on MTF-free RF decoded with `-m 0` may be outside its
   design envelope, not a defect.
2. **The disc baselines depend on models that are anchored only in part:**
   - the DdD capture filter, fitted to one board's measurement; other
     boards differ by their component tolerances;
   - S11's linear readout near the optical cut-off. At 55 mm and on CLV, the
     upper chroma sidebands of all but the darkest levels lie at or past
     0.833·fc. Not even the reference decoder recovers them (CLV: C1 −54.6 %,
     the loss of one sideband), so the RF there cannot be checked against
     the truth this way.
3. **The standard's nominal values are not real discs.** ld-creator follows
   the IEC 60857 nominals (pre-emphasis, pre-distortion), within their
   tolerances. A decoder tuned on real discs is entitled to match those
   discs, and real discs differ from each other: by about 3 dB in recorded
   chroma and about 30 ns in chroma timing among the six compared
   ([Other discs](#other-discs)).
4. **The real discs' truth is unknown.** Each disc's VITS were generated and
   mastered with their own errors, so a measurement on its decode is the
   disc and the decoder together. The discs can show that ld-decode behaves
   the same on ld-creator's RF as on theirs, but not which is right.

## Unattributed discrepancies

### U1: Chroma level on MTF-free RF and on CLV

On the CAV disc baselines U1 is resolved: it came from the truth's burst
(above). Two cases remain:

- **CLV.** C1 is −3.8 % and F3g −3.0 %, with F1 3.58 MHz confirmed
  (−0.32 dB). That is within what reason 2 leaves uncertain: on CLV the
  upper chroma sidebands lie near the optical cut-off. ld-decode refines its
  burst estimate there in two steps (17.9, then 19.3 IRE), and settles on
  chroma strength 0.764. On the two real CLV discs ld-decode reads VIRS
  chroma at 38.3 and 39.3 IRE p-p (−4.3 and −1.8 % of nominal), but their
  recorded level is unknown (reason 4).
- **The carrier diagnostic.** ld-decode's chroma still reads high: C1
  +4.4 %, C3a +2.5 to +5.3 % and F3g +2.4 %, with F1 3.58 MHz now confirmed
  (+0.39 dB). With the short burst it read C1 +15.6 %. Without the automatic
  correction it reads C1 −9.3 %. So the correction, at chroma strength
  0.735, lifts chroma by about 15 % where 10 % would restore it.

- **Points towards the decoder:** the FM signal is shown correct (the
  reference decoder's chroma is within 0.2 %), and the correction overshoots
  on MTF-free RF.
- **Points away from it:** reason 1 for the carrier diagnostic, which no
  real capture can settle, since no real RF is MTF-free. Reason 2 for CLV.

### U2: Chroma-to-luma delay

| Run | F3d |
|-----|-----|
| ld-decode, carrier diagnostic, S3 on | +35.5 ns |
| ld-decode, carrier diagnostic, S3 off | +116.7 ns |
| Reference decoder, carrier diagnostic, with the §9.1.7 equaliser | +2.8 ns |
| ld-decode, disc baselines: 55 mm, 145 mm, CLV | +12.5, +17.6, +13.4 ns |

ld-decode delays chroma about 116 ns relative to luma, where the
pre-distortion presumes 80 ± 15 ns. On disc RF the delay is smaller. At
55 mm it is within the 15 ns threshold on all but one frame, and at 145 mm
it is outside on every frame.

The 12.5T chroma − luma delay in ld-decode's decode of GGV1069 is
+26 ± 9 ns (inner), +17 ± 7 ns (middle) and +13 ± 5 ns (outer), against
+12.9, +15.7 and +16.7 ns for ld-creator's RF at the same radii. Through
the IEC chain, GGV1069's RF and ld-creator's differ by 2.4 ns and 0.3 ns
at 74.6 and 96.3 mm (13 ns at 55 mm, within GGV1069's ±7 ns spread).

- **Points towards the decoder:** the residual on the carrier diagnostic is
  outside §9.1.7's tolerance, and S3 is shown correct. On GGV1069, whose RF
  timing matches ld-creator's, ld-decode shows the same delay.
Across the six discs ([Other discs](#other-discs)) the RF timing through
the IEC chain is not one value. Five discs read −8 to −15 ns and two read
−40 to −43 ns. ld-decode's decode shifts every disc, and ld-creator's RF,
by a similar +34 to +42 ns, so ld-decode treats ld-creator's RF as it
treats real discs. The first group decodes at +22 to +33 ns, outside the
15 ns threshold, and the second at −8 to 0 ns. ld-creator's RF, at
−24.9 ns, lies between them.

- **Points towards the decoder:** the residual on the carrier diagnostic is
  outside §9.1.7's tolerance, and S3 is shown correct. ld-decode shifts
  ld-creator's RF by the same amount as every real disc.
- **Points away from it:** the discs' own 12.5T timing is unknown (reason
  4). The two groups differ by about 30 ns, so discs do not agree on a
  timing that ld-decode could be checked against.
- **Would settle it:** a disc whose VITS chroma-luma timing is known.

### U3: Response at 3–4.2 MHz on MTF-free RF

With `--no_auto_chroma`, ld-decode's multiburst on the carrier diagnostic
reads −0.28, −0.84 and −2.77 dB at 3, 3.58 and 4.2 MHz. The reference
decoder reads +0.01, +0.03 and +0.04 dB on the same RF.

On disc RF, ld-decode's response with `--no_auto_chroma` falls the same way
on both. At the inner radius ld-creator's RF and GGV1069 agree (3.58 MHz:
−1.15 and −1.12 dB re 1 MHz; 4.2 MHz: −2.14 and −2.02 dB). At 74.6 and
96.3 mm ld-creator's falls about 1.2–1.4 dB further, the chroma-band
difference above. With its defaults, ld-decode's automatic correction lifts
both.

**Points away from the decoder:** reason 1. ld-decode's video filtering
presumably works together with its MTF compensation, which `-m 0` removes.

### U4: Chroma phase that varies with luminance (differential phase)

| Run | C2 (VIRS) | C3p (staircase) |
|-----|-----------|-----------------|
| ld-decode, carrier diagnostic | +2.2° | −0.1° to +2.4° |
| Reference decoder, carrier diagnostic | +0.05° | −0.3° to +0.5° |
| ld-decode, disc baselines: 55 mm, 145 mm, CLV | +1.0°, +0.6°, +1.1° | −0.8° to +2.7° |

The staircase amplitude (C3a) also varies with luminance on disc RF, by
−1.9 to +3.2 % at 55 mm, −0.4 to +3.3 % at 145 mm and −5.5 to +3.8 % on
CLV.

ld-decode's chroma differential-gain servo stayed off ("chroma DG off") on
ld-creator's RF; on GGV1069's inner excerpt it settled at −0.00101/IRE.
ld-decode's decode of GGV1069 shows a staircase differential phase of
4.1–6.0° (max − min) and differential gain of 5.3–8.5 %, against 1.6–2.7°
and 2.8–3.7 % for ld-creator's RF at the same radii. GGV1069's decode is
noisy (±1–2° per frame) and its VITS truth is unknown (reason 4).

Every disc in the survey ([Other discs](#other-discs)) decodes with more
differential phase and gain than ld-creator's RF: DP 5.3–9.9° and DG
7.0–16.9 %, against 2.7° and 3.7 %. Through the IEC chain, without
compensation, the difference does not show: ld-creator's RF reads 9.9° of
DP and the discs 6.8–9.9°. That measurement is dominated by the
uncompensated readout, so it cannot show whether the discs carry
differential phase that ld-creator's ideal disc does not. The other
possibility is that ld-decode's compensation removes the chain's
differential phase from ld-creator's RF but not from real RF.

**Would settle it:** a disc whose staircase differential phase is known, or
an IEC-chain measurement with the readout compensated, applied to both.

## Reproducing

```bash
nix develop "git+file://$PWD" --command cmake --build build
nix develop "git+file://$PWD" --command python3 -E -B scripts/round_trip.py
```

`results.md` holds, for each baseline, the ld-decode table and the reference
decoder's table. The bisection runs are single changes to a baseline:

- **Decoded in `build/round_trip/`.** Each result is scored with
  `python3 -E -B scripts/cvbs_compare.py build/round_trip/round_trip.cvbs <decoded>.cvbs`.
- **Reference decodes** are `scripts/reference_demod.py <rf.s16> round_trip.cvbs <out.cvbs>`.

| Bisection | Change |
|-----------|--------|
| Standard burst (earlier truth only) | `scripts/standard_burst.py` on the truth into a new directory, then each round-trip project with `--output-root` there; ld-decode as in the round trip, scored against the copy. The pinned videosynth's burst is already standard |
| Without the automatic chroma correction | `ld-decode --NTSC -m 0 --no_auto_chroma carrier.s16 carrier_noac` |
| Carrier diagnostic without S3 | `ntsc_round_trip_carrier.yaml` plus `group_delay_predistorter: {enabled: false}` and `allow_out_of_tolerance: [group_delay_predistorter.enabled]`; decoded with `-m 0`, with and without `--no_auto_chroma` |
| Ideal capture | a disc baseline's project plus `rf_capture: {profile: ideal}`; ld-decode with its defaults, the reference decoder with `--playback-equaliser --mtf <resolved project>` |
| Reference without the playback equaliser | the reference decoder without `--playback-equaliser` |
| MTF bisection run | `scripts/round_trip.py --baselines cav_inner_equalised` |

The real-capture comparison ([scripts README](../../scripts/README.md#real-captures)):

1. Cut the excerpts:
   `ld-cut -n -s <300|12000|25000> -l 40 <capture.ldf> build/ggv1069/real_<excerpt>.s16`.
2. Make ld-creator RF at 74.6 and 96.3 mm: `ntsc_round_trip_cav_outer.yaml`
   with `start_radius_mm` changed, run with `--output-root` on a directory
   holding the round-trip truth. The 55 mm RF is `cav_inner.s16`.
3. Decode each with `ld-decode --NTSC`, and with `--no_auto_chroma`.
4. Measure the decodes with `vits_measure.py cvbs <decoded.cvbs> --frames 30`,
   and the RF with
   `vits_measure.py rf <rf.s16> --frames 10 --playback-equaliser [--sidebands]`.

The other discs: `scripts/capture_survey.py --output-dir build/captures
--start 1000 --frames 40 "<label>=<capture or .s16>" …`, with ld-creator's
`build/round_trip/cav_inner.s16` as one source. It cuts, decodes and measures
each source as in steps 1, 3 and 4, and writes the table to
`build/captures/survey.md`.
