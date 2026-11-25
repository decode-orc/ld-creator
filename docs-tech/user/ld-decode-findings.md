# ld-decode findings

The discrepancies the NTSC round trip has found, their evidence, and how
they are attributed (plan Task 7.5). The measurements, thresholds and triage
classes are defined in [ntsc-round-trip-metrics.md](ntsc-round-trip-metrics.md),
and the baseline tables are in its "Results" section.

**No ld-decode finding is established.** The discrepancies below are
**unattributed**. ld-creator's RF carries the IEC 60857 signal, and ld-decode
disagrees with the truth, but ld-decode is calibrated on real discs, not on
the standard. Until a real capture shows how ld-creator's RF compares with a
real disc, a disagreement may come from ld-decode or from a gap between the
standard's nominal disc and real ones.

Everything about ld-decode is taken from its external interface: its command
line, its CVBS output, and the capture notes and log it writes. ld-decode's
source was not read.

## Versions

| Tool | Version |
|------|---------|
| ld-decode | `8.0.0pre1+git.d6055b3` (flake pin `d6055b303f67869d005a23619e886453ff73130d`) |
| videosynth | `a995726` (flake pin `a9957262e9aa23ffa5d89cb2a66760506db48462`) |
| ld-creator | `c647379` plus the Phase 7 working tree |

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
| CAV 145 mm, DdD capture (not compensated) | ±0.1 IRE | −18.6 % | −2.8° | −16 to −20 % | −1.79 dB | −1.33 dB | −18.4 % | −15.3 ns | refuted |

So:

- **S2–S5 are right end to end.** The carrier diagnostic decodes to the
  truth within every judged threshold. With no playback equaliser, chroma
  arrives 82 ns early: that is S3's §9.1.7 pre-distortion, which a player is
  meant to cancel.
- **S6–S13 are right where the readout does not intervene.** At 145 mm the
  optical cut-off is 34.5 MHz, far above every sideband. There the disc RF
  with the `ideal` capture decodes to the truth as well.
- **The modelled DdD capture filter alone moves chroma by about 18 %.** It
  does so in a band where the model is known to be 0.5–2.9 dB off the
  measured board (plan Task 5.3). Any decoder of DdD captures has to
  compensate that response, and ld-decode's compensation was set on real DdD
  captures. So on disc RF through the DdD profile, ld-decode's output depends
  on how closely the modelled filter matches the real one.

### Levels and VBI codes

ld-decode recovers every level within 0.14 IRE and every VBI code exactly,
on all four baselines.

### Two errors on the round trip's side, corrected

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

## Why the remaining discrepancies are not attributed

1. **The carrier diagnostic is not an input ld-decode is built for.** Real RF
   always carries the readout MTF, and ld-decode's MTF compensation and
   burst-driven chroma correction presumably work as a system on real RF.
   Its behaviour on MTF-free RF decoded with `-m 0` may be outside its
   design envelope, not a defect.
2. **The disc baselines depend on two models that are not yet anchored to
   real discs:**
   - the DdD capture filter (above);
   - S11's linear readout near the optical cut-off. At 55 mm and on CLV, the
     upper chroma sidebands of all but the darkest levels lie at or past
     0.833·fc. Not even the reference decoder recovers them (CLV: C1 −49.7 %,
     the loss of one sideband), so the RF there cannot be checked against
     the truth this way.
3. **The standard's nominal values are not real discs.** ld-creator follows
   the IEC 60857 nominals (pre-emphasis, pre-distortion), within their
   tolerances. A decoder tuned on real discs is entitled to match those
   discs.

## Unattributed discrepancies

Each discrepancy lists what would settle it. "Real capture" means a DomesdayDuplicator capture
of an NTSC disc with VITS. ld-decode's calibration log (burst level, MTF
level, video EQ, chroma strength) and `--verboseVITS` output on it would
show whether ld-creator's RF settles ld-decode the way a real disc does.

### U1: Chroma level

ld-decode's chroma reads high on every baseline:

| Run | C1 |
|-----|----|
| Carrier diagnostic | +15.6 % |
| Carrier diagnostic, `--no_auto_chroma` | −9.3 % |
| CAV 55 mm | +5.6 % |
| CAV 145 mm | +5.8 % |
| CLV | +5.1 % |

With its defaults, ld-decode sets an inverse-MTF chroma filter from the
colour burst. On the carrier diagnostic, it reports the uncorrected burst at
15.65 IRE against 20 nominal and adds 2.11 dB at fsc. The burst it actually
outputs uncorrected is 17.4 IRE, against 19.7 in the truth (−1.1 dB). On
the same RF, the reference decoder's chroma is within 0.1 %.

- **Points towards the decoder:** on the carrier diagnostic, the FM signal is
  shown correct and the correction overshoots.
- **Points away from it:** reason 1, and on disc RF reason 2 (C1 at 55 mm is
  +11.3 % with the `ideal` capture, against +5.6 % with the DdD).
- **Would settle it:** the calibration values ld-decode reports on a real
  capture, compared with ours; and the DdD filter gap closed (Task 5.3).

### U2: Chroma-to-luma delay

| Run | F3d |
|-----|-----|
| ld-decode, carrier diagnostic, S3 on | +35.4 ns |
| ld-decode, carrier diagnostic, S3 off | +117.0 ns |
| Reference decoder, carrier diagnostic, with the §9.1.7 equaliser | +2.9 ns |

ld-decode delays chroma about 116 ns relative to luma, where the
pre-distortion presumes 80 ± 15 ns. The disc baselines show +4 ns at 55 mm,
+17 ns at 145 mm and +3 ns on CLV, so the decoder's delay changes with its
MTF compensation.

- **Points towards the decoder:** the residual on the carrier diagnostic is
  outside §9.1.7's tolerance, and S3 is shown correct.
- **Points away from it:** real discs or players may not follow §9.1.7's
  nominal curve, and ld-decode may be tuned to them.
- **Would settle it:** the 12.5T chroma-to-luma delay that ld-decode measures
  on a real capture's VITS.

### U3: Response at 3–4.2 MHz on MTF-free RF

With `--no_auto_chroma`, ld-decode's multiburst on the carrier diagnostic
reads −0.27, −0.83 and −2.8 dB at 3, 3.58 and 4.2 MHz. The reference
decoder reads +0.01, +0.02 and +0.04 dB on the same RF.

**Points away from the decoder:** reason 1. ld-decode's video filtering
presumably works together with its MTF compensation, which `-m 0` removes.

### U4: Chroma phase that varies with luminance (differential phase)

| Run | C2 (VIRS) | C3p (staircase) |
|-----|-----------|-----------------|
| ld-decode, carrier diagnostic | +2.0° | −0.2° to +2.3° |
| Reference decoder, carrier diagnostic | +0.0° | −0.4° to +0.5° |

The disc baselines show a similar spread. ld-decode's chroma
differential-gain servo stayed off ("chroma DG off").

**Would settle it:** the differential phase that ld-decode measures on a
real capture's modulated staircase.

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
| Without the automatic chroma correction | `ld-decode --NTSC -m 0 --no_auto_chroma carrier.s16 carrier_noac` |
| Carrier diagnostic without S3 | `ntsc_round_trip_carrier.yaml` plus `group_delay_predistorter: {enabled: false}` and `allow_out_of_tolerance: [group_delay_predistorter.enabled]`; decoded with `-m 0`, with and without `--no_auto_chroma` |
| Ideal capture | a disc baseline's project plus `rf_capture: {profile: ideal}`; ld-decode with its defaults, the reference decoder with `--playback-equaliser --mtf <resolved project>` |
| Reference without the playback equaliser | the reference decoder without `--playback-equaliser` |
| MTF bisection run | `scripts/round_trip.py --baselines cav_inner_equalised` |
