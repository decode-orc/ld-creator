# ld-decode findings

The discrepancies the NTSC round trip has found, their evidence, and how
they are attributed (plan Task 7.5). The measurements, thresholds and triage
classes are defined in [ntsc-round-trip-metrics.md](ntsc-round-trip-metrics.md),
and the baseline tables are in its "Results" section.

**No ld-decode finding is established.** The largest discrepancy, ld-decode's
high chroma level on disc RF (U1), came from the round trip's truth: the
pinned videosynth's colour burst is shorter than the standard allows
([The colour burst](#the-colour-burst-u1-on-disc-rf)). The others remain
**unattributed**. ld-creator's RF carries the IEC 60857 signal, and ld-decode
disagrees with the truth, but ld-decode is calibrated on real discs, not on
the standard. A real capture has now been compared
([The real capture](#the-real-capture)), but it is one disc, and mastering
differed between discs and between companies, so it shows how ld-creator's
RF compares with that disc, not with real discs in general.

Everything about ld-decode is taken from its external interface: its command
line, its CVBS output, and the capture notes and log it writes. ld-decode's
source was not read.

## Versions

| Tool or source | Version |
|----------------|---------|
| ld-decode | `8.0.0pre1+git.d6055b3` (flake pin `d6055b303f67869d005a23619e886453ff73130d`) |
| videosynth | `a995726` (flake pin `a9957262e9aa23ffa5d89cb2a66760506db48462`) |
| ld-creator | `b16aedc` plus the working tree |
| Real capture | Pioneer GGV1069 (NTSC CAV test disc), side 1, DomesdayDuplicator capture of 2019, 21.6 GB `.ldf`; not in the repository |

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
| Carrier diagnostic | ±0.1 IRE | +0.1 % | +0.0° | ±0.6 % | +0.02 dB | +0.04 dB | −0.1 % | +2.9 ns | confirmed |
| Carrier diagnostic, no playback equaliser | ±0.1 IRE | +0.1 % | −0.3° | ±1.1 % | +0.06 dB | −0.02 dB | −0.1 % | −82.3 ns | F3d refuted |
| CAV 145 mm, `ideal` capture | ±0.03 IRE | +0.1 % | −0.0° | ±0.3 % | +0.02 dB | +0.03 dB | −0.0 % | +3.0 ns | confirmed |
| CAV 145 mm, DdD capture (not compensated) | ±0.07 IRE | −15.2 % | −2.1° | −13 to −17 % | −1.49 dB | −1.21 dB | −15.7 % | −16.8 ns | refuted |

So:

- **S2–S5 are right end to end.** The carrier diagnostic decodes to the
  truth within every judged threshold. With no playback equaliser, chroma
  arrives 82 ns early: that is S3's §9.1.7 pre-distortion, which a player is
  meant to cancel.
- **S6–S13 are right where the readout does not intervene.** At 145 mm the
  optical cut-off is 34.5 MHz, far above every sideband. There the disc RF
  with the `ideal` capture decodes to the truth as well.
- **The DdD capture filter alone moves chroma by about 15 %.** Any decoder
  of DdD captures has to compensate that response, and ld-decode's was set
  on real DdD captures. The filter is fitted to a real board's measured
  response, within 0.17 dB and 6° (S13 note, "The fit").

### Levels and VBI codes

ld-decode recovers every level within 0.14 IRE and every VBI code exactly,
on all four baselines.

### Errors on the round trip's side, corrected or attributed

- **The colour burst (U1 on disc RF).** The truth's burst is about 7.3
  cycles long; ld-decode's chroma correction reads it low and over-corrects.
  See the next section.
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

## The colour burst (U1 on disc RF)

IEC 60857 §9.1.2 calls for the "standard EIA recommended burst" (EIA
Television Systems Bulletin No. 4). ITU-R BT.470-6 Table 2 items 2.14g/h
give it for M/NTSC: it starts 5.3 µs after 0H (4.71–5.71 µs) and lasts
9 ± 1 cycles (2.23–3.11 µs), both at the envelope's 50 % points.
[`vits_measure.py`](../../scripts/README.md#real-captures) measures:

| Source | Burst start | Duration |
|--------|-------------|----------|
| Truth (pinned videosynth) | 5.63 µs | 7.25 cycles |
| GGV1069, ld-decode's decode, all three excerpts | 5.33–5.35 µs | 8.91 cycles |
| BT.470-6 | 5.3 µs | 9 cycles (8–10) |

videosynth's burst gate is 5.3–7.97 µs, but its envelope's ramps lie inside
the gate, so the 50 % points fall inside it too
([videosynth#32](https://github.com/decode-orc/videosynth/issues/32)).

ld-decode sets its inverse-MTF chroma strength from the burst it measures.
On the short burst it measures too little burst: on the carrier diagnostic
its log reports the uncorrected burst at 15.65 IRE, where its own
uncorrected output (`--no_auto_chroma`) carries 17.4 IRE. On the real
disc the two agree (17.3 and 17.2 IRE at the inner excerpt).

**Bisection.** [`standard_burst.py`](../../scripts/README.md#real-captures)
copies the truth with every burst replaced by the BT.470-6 burst, with the
same amplitude and phase and 300 ns edges, and nothing else changed. The
four baselines rerun on the copy:

| Baseline | ld-decode's burst estimate | Chroma strength | C1 | C2 | C3a | F1 3.58 MHz | F3g | F3d |
|----------|----------------------------|-----------------|----|----|-----|-------------|-----|-----|
| Carrier diagnostic (`-m 0`) | 17.0 IRE | 0.832 | +6.4 % | +2.1° | +4.7 to +6.8 % | +0.56 dB | +4.3 % | +35.3 ns |
| CAV 55 mm | 17.2 IRE | 0.788 | **+0.7 %** | +0.5° | +0.0 to +5.4 % | **+0.11 dB** | **+1.2 %** | +12.5 ns |
| CAV 145 mm | 16.6 IRE | 0.978 | +2.5 % | +0.6° | +1.5 to +5.2 % | **+0.19 dB** | **+0.7 %** | +17.6 ns |
| CLV | 17.6 IRE | 0.666 | −2.2 % | +0.7° | −3.6 to +5.6 % | **−0.15 dB** | **−1.2 %** | +13.1 ns |

Bold values are within their thresholds; F3d is unchanged (U2). With the
short burst, the same baselines gave C1 +15.6, +9.5, +11.3 and +6.9 %.

ld-decode's estimate also depends on the burst's edges: the same copy with
faster edges (180 ns, 10–90 %) gave 17.3 IRE and chroma strength 0.752 at
55 mm, and C1 +0.02 %, so its burst measurement integrates over a window
that the burst's edges reach into.

ld-decode's calibration at 55 mm is now close to the real disc's at its
inner radius (CAV frames 18–57): there it measures the uncorrected burst at
17.3 IRE and sets chroma strength 0.751.

**Class: Measurement** (the truth is wrong; recorded as a videosynth
defect). The round trip's recorded results keep the pinned videosynth until
its burst is corrected and the pin updated. What remains on the carrier
diagnostic is U1 below.

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
(the round-trip CAV project with only `start_radius_mm` changed) has the
same carrier envelope slope, blanking (8.1 MHz) against sync tip
(7.6 MHz), as the excerpt within 0.03: 0.856 against 0.882, 0.936 against
0.939, and 0.969 against 0.963. So the radii match.

GGV1069 carries the NTC-7 composite on line 13, the NTC-7 combination on
line 276 and VIRS on lines 19 and 282, with 7.5 IRE setup. Its VITS chroma
is locked to the burst (+2° to +5°). The truth's is not: its phase against
the burst alternates by 180° from line to line
([videosynth#33](https://github.com/decode-orc/videosynth/issues/33)), and
its VIRS has no setup ([videosynth#31](https://github.com/decode-orc/videosynth/issues/31)).
Neither affects the round-trip metrics, which compare with the truth line by
line.

### ld-decode's calibration

| RF | Uncorrected burst (log) | MTF level | Chroma strength |
|----|-------------------------|-----------|-----------------|
| GGV1069 inner | 17.3 IRE | 0.987 | 0.751 |
| GGV1069 middle | 18.8 IRE | 0.776 | 0.314 |
| GGV1069 outer | 18.9 IRE | 0.654 | 0.290 |
| ld-creator 55 mm, short burst | 15.8 IRE | 1.000 | 1.223 |
| ld-creator 74.6 mm, short burst | 15.6 IRE | 0.767 | 1.296 |
| ld-creator 96.3 mm, short burst | 15.4 IRE | 0.618 | 1.360 |
| ld-creator 55 mm, standard burst | 17.2 IRE | 1.000 | 0.788 |

ld-decode's MTF level follows the radius on both alike.

### Through the IEC chain, without compensation

`vits_measure.py rf` demodulates the RF by IEC 60857 alone, with the
playback equaliser but no readout or capture compensation, so the readout
and capture chain stays in the result. ld-creator RF and GGV1069 at the
same radius:

| Quantity | 55 mm: ld-creator / GGV1069 | 74.6 mm | 96.3 mm |
|----------|-----------------------------|---------|---------|
| Burst (IRE p-p) | 50.1 / 48.9 | 36.8 / 41.9 | 34.3 / 39.7 |
| Multiburst 3.58 MHz re 1 MHz | +2.71 / +2.41 dB | −0.30 / +0.87 dB | −0.99 / +0.51 dB |
| Multiburst 4.2 MHz re 1 MHz | +3.81 / +3.60 dB | +0.11 / +1.60 dB | −0.74 / +1.23 dB |
| 12.5T chroma / luma | 1.39 / 1.38 | 0.94 / 1.09 | 0.86 / 1.02 |
| 12.5T chroma − luma delay | −25.5 / −12.0 ± 7 ns | −23.4 / −20.8 ± 8 ns | −21.3 / −21.1 ± 5 ns |
| Differential phase, max − min | 9.4° / 6.9° | 7.6° / 8.9° | 6.5° / 9.2° |

At 55 mm the two agree within 0.3 dB. At 74.6 and 96.3 mm GGV1069 carries
1.2–1.5 dB more chroma than ld-creator. A fit of the FM sidebands
(`--sidebands`) locates the difference: the difference between the lower
and upper sideband of a packet, which measures the chain's tilt and not
the modulation depth, agrees within about ±0.4 dB at 74.6 mm, and at
96.3 mm GGV1069's chain is if anything steeper. The sidebands' sum, relative
to the carrier, is larger on GGV1069 by an amount that grows with the video
frequency and is the same at both radii: −0.3 dB at 1 MHz, +0.6 dB at 2 MHz,
+1.4 dB at 3 MHz and +1.2 to +1.8 dB at 3.58 and 4.2 MHz. So GGV1069's chroma
band is recorded with more FM deviation than ld-creator's IEC-nominal
§9.2.4 pre-emphasis gives, or its readout's response differs in curvature
over 4.5–13 MHz. The second sidebands, which would separate the two, are
too strongly shaped by the chain's tilt to do so. This is one disc's
mastering; it is recorded as an observation, not a model defect. With
the defaults, ld-decode decodes GGV1069's chroma level to nominal at all
three radii (staircase 40.5–42.6 IRE p-p, VIRS 38.1–39.8 IRE p-p, against
40), so its correction absorbs the difference.

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
     0.833·fc. Not even the reference decoder recovers them (CLV: C1 −49.7 %,
     the loss of one sideband), so the RF there cannot be checked against
     the truth this way.
3. **The standard's nominal values are not real discs.** ld-creator follows
   the IEC 60857 nominals (pre-emphasis, pre-distortion), within their
   tolerances. A decoder tuned on real discs is entitled to match those
   discs, and real discs differed from each other.
4. **One real disc's truth is unknown.** GGV1069's VITS were generated and
   mastered with their own errors, so a measurement on its decode is the
   disc and the decoder together. It can show that ld-decode behaves the
   same on ld-creator's RF as on that disc's, but not which is right.

## Unattributed discrepancies

### U1: Chroma level on MTF-free RF, and its residual on disc RF

On disc RF, U1 came from the truth's burst (above). With the standard
burst, C1 is +0.7 % at 55 mm but still +2.5 % at 145 mm and −2.2 % on CLV,
just outside ±2 %, with F1 3.58 MHz and F3g confirmed. Those residuals are
within what reason 2 leaves uncertain (the DdD filter fit, and on CLV the
upper sidebands near the optical cut-off) and stay unattributed. On the carrier
diagnostic, with the standard burst, ld-decode's chroma still reads high:
C1 +6.4 %, C3a +4.7 to +6.8 %, F1 3.58 MHz +0.56 dB and F3g +4.3 % (with the
short burst: +15.6 %, +15.0 %, and so on; without the automatic correction,
C1 −9.3 %). ld-decode sets MTF level 1.500 and chroma strength 0.832 there.

- **Points towards the decoder:** the FM signal is shown correct (the
  reference decoder's chroma is within 0.1 %), and the correction overshoots.
- **Points away from it:** reason 1. No real RF is MTF-free, so a real
  capture cannot settle it.

### U2: Chroma-to-luma delay

| Run | F3d |
|-----|-----|
| ld-decode, carrier diagnostic, S3 on | +35.4 ns |
| ld-decode, carrier diagnostic, S3 off | +117.0 ns |
| Reference decoder, carrier diagnostic, with the §9.1.7 equaliser | +2.9 ns |
| ld-decode, standard burst: 55 mm, 145 mm, CLV | +12.5, +17.6, +13.1 ns |

ld-decode delays chroma about 116 ns relative to luma, where the
pre-distortion presumes 80 ± 15 ns. On disc RF the delay is smaller, and the
standard burst does not change it; at 55 mm it is now within the 15 ns
threshold on every frame, at 145 mm not.

The 12.5T chroma − luma delay in ld-decode's decode of GGV1069 is
+26 ± 9 ns (inner), +17 ± 7 ns (middle) and +13 ± 5 ns (outer), against
+12.2, +14.9 and +16.4 ns for ld-creator's RF at the same radii. Through
the IEC chain, GGV1069's RF and ld-creator's differ by 2.6 ns and 0.2 ns
at 74.6 and 96.3 mm (13 ns at 55 mm, within GGV1069's ±7 ns spread).

- **Points towards the decoder:** the residual on the carrier diagnostic is
  outside §9.1.7's tolerance, S3 is shown correct, and on GGV1069, whose RF
  timing matches ld-creator's, ld-decode shows the same delay.
- **Points away from it:** GGV1069's own 12.5T timing is unknown (reason
  4), and other discs or players may not follow §9.1.7's nominal curve.
- **Would settle it:** a disc whose VITS chroma-luma timing is known, or
  several discs from different mastering facilities agreeing.

### U3: Response at 3–4.2 MHz on MTF-free RF

With `--no_auto_chroma`, ld-decode's multiburst on the carrier diagnostic
reads −0.27, −0.83 and −2.8 dB at 3, 3.58 and 4.2 MHz. The reference
decoder reads +0.01, +0.02 and +0.04 dB on the same RF.

On disc RF ld-decode's response with `--no_auto_chroma` falls the same way on
both. At the inner radius ld-creator's RF and GGV1069 agree (3.58 MHz:
−1.11 and −1.12 dB re 1 MHz; 4.2 MHz: −2.14 and −2.02 dB). At 74.6 and
96.3 mm ld-creator's falls about 1.2 dB further, the chroma-band difference
above. With its defaults ld-decode's automatic correction lifts both.

**Points away from the decoder:** reason 1. ld-decode's video filtering
presumably works together with its MTF compensation, which `-m 0` removes.

### U4: Chroma phase that varies with luminance (differential phase)

| Run | C2 (VIRS) | C3p (staircase) |
|-----|-----------|-----------------|
| ld-decode, carrier diagnostic | +2.0° | −0.2° to +2.3° |
| Reference decoder, carrier diagnostic | +0.0° | −0.4° to +0.5° |
| ld-decode, standard burst: 55 mm, 145 mm, CLV | +0.5°, +0.6°, +0.7° | −1.1° to +2.2° |

The staircase amplitude (C3a) also varies with luminance on disc RF: with
the standard burst, by +0.0 to +5.4 % at 55 mm, +1.5 to +5.2 % at 145 mm and
−3.6 to +5.6 % on CLV.

ld-decode's chroma differential-gain servo stayed off ("chroma DG off") on
ld-creator's RF; on GGV1069's inner excerpt it settled at −0.00101/IRE.
ld-decode's decode of GGV1069 shows a staircase differential phase of
4.1–6.0° (max − min) and differential gain of 5.3–8.5 %, against 1.4–2.7°
and 3.2–4.0 % for ld-creator's RF at the same radii. GGV1069's decode is
noisy (±1–2° per frame) and its VITS truth is unknown (reason 4).

**Would settle it:** a disc whose staircase differential phase is known.

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
| Standard burst | `scripts/standard_burst.py` on the truth into a new directory, then each round-trip project with `--output-root` there; ld-decode as in the round trip, scored against the copy |
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
