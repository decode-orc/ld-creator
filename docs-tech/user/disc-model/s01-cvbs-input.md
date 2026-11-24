# S1 — CVBS input (`cvbs_input`)

## Physical process

The programme source: the composite video handed to the mastering facility.
Here it is the exact, synthetic recording from videosynth, so it is the
**truth** that the round trip compares against.

## Sources

- CVBS file format specification:
  [index.md](../../cvbs-file-format-specification/docs/index.md) (metadata,
  stored frame and line origin),
  [video-standard-presets.md](../../cvbs-file-format-specification/docs/video-standard-presets.md)
  (`NTSC` preset), and
  [sample-encoding-presets.md](../../cvbs-file-format-specification/docs/sample-encoding-presets.md).
- [SMPTE 244M-2003](../../analogue-video-specifications/docs/video_formats/SMPTE-244M-2003/SMPTE-244M-2003.md)
  §4.2.1: blanking = 240, white = 800 (10-bit).
- [SMPTE 170M-2004](../../analogue-video-specifications/docs/video_formats/SMPTE-170M-2004/SMPTE-170M-2004.md):
  IRE scale, fH = 4.5 MHz / 286, fsc = 455/2 · fH.
- IEC 60857-1986 §9.1.6 note: 100 IRE is the amplitude from blanking to
  white.

## Model and equations

**Decoding to the 10-bit domain.** Each sample encoding defines how a stored
word maps back to the 10-bit value `v` (sample-encoding presets):

| Encoding | `v` from stored word `w` |
|----------|--------------------------|
| `CVBS_U10_4FSC` | `v = w` (signed 16-bit container) |
| `CVBS_U16_4FSC` | `v = w / 64` |
| `CVBS_S16_4FSC` | `v = w / 32 + blanking` |
| `CVBS_TPG21_4FSC` | `v = w / 64 + 508` |

`v` is kept as a `double`. `CVBS_S16_4FSC` and `CVBS_U10_4FSC` can carry
values outside 0–1023, which the LaserVision VBI burst needs (it dips below
sync tip). Those values are carried through, not clamped.

**Conversion to IRE.** By the IEC 60857 §9.1.6 note, 100 IRE is the
blanking-to-white amplitude, so

```
IRE = 100 · (v − blanking) / (white − blanking)
```

With the preset levels (blanking 240, white 800) this is `(v − 240) / 5.6`.
Black is not used: it is a picture level (+7.5 IRE setup), not a reference of
the scale. The only level override the CVBS spec's metadata defines is
`black_level`, so no override affects the conversion.

**Accepted input.** The `.meta` must declare `preset` `NTSC`, `signal_type`
`composite`, a time-base-stable 4fsc signal state (`STANDARD_STABLE_LOCKED`
or `STANDARD_STABLE_UNLOCKED`, so that the 910-sample line and the stored
origin below hold), and one of the four encodings above. The `.cvbs` must
hold whole 477 750-sample frames, matching `number_of_sequential_frames`
when it is given.

Check values: sync tip 16 → −40 IRE; black 282 → +7.5 IRE; white 800 → 100
IRE; peak 1019 → 139.1 IRE.

**Time origin.** By the CVBS spec's stored-origin rule (NTSC preset, "Stored
line alignment"), sample 0 of each stored line is the first sampling instant
at or after that line's 0H, and 0H falls 57/90 of a sample period before it.
Lines are 910 samples and frames 525 lines, with no gaps. So stored sample *n*
of the file (counting from the first frame read) is at

```
t_n = (n + 57/90) / 4fsc,   4fsc = 4 · 455/2 · 4.5 MHz / 286 = 14 318 181.818… Hz
```

with `t = 0` at 0H of frame line 1 of the first frame read.

## Parameters

| Register row | Value |
|--------------|-------|
| `S1.fh` | 4.5 MHz / 286 |
| `S1.fsc` | 455/2 · fH |
| `S1.sample_rate` | 4 fsc |
| `S1.blanking_code`, `S1.white_code` | 240, 800 (or `.meta` override) |
| `S1.ire_scale` | 100 IRE = white − blanking |
| `S1.h_origin` | 57/90 sample after 0H |

## Assumptions and approximations

- The input is NTSC with orthogonal 4fsc sampling. Anything else is rejected
  (the `.meta` video standard must be `NTSC`), so no approximation arises.
- Composite (`.cvbs`) only. Y/C input is rejected: a LaserVision disc records
  composite (HLD §2.1).
- The conversion is exact in `double` precision: the error is below 10⁻¹² IRE.

## Input/output contract

- **In:** `<base>.cvbs` and `<base>.meta`, as named by `input.video_path`.
- **Out:** D1 — IRE samples at 4fsc, time origin as above.
- A short final frame is an error naming the frame, not a silent pad.

## Project YAML block

`input` (see [docs/reference/project-file.md](../../../docs/reference/project-file.md#input)):
`video_path` (required), `first_frame` (default 0) and `frame_count`
(default: every frame from `first_frame`).

## Ideal setting

Not applicable: S1 is a conversion with no physical impairment.

## Verification

Unit tests, per encoding: sync tip, blanking, black and white map to −40, 0,
7.5 and 100 IRE within 10⁻⁹. A below-sync value from `CVBS_S16_4FSC` keeps
its sign. A truncated frame is reported.

## Diagnostic tap

`cvbs_input`: D1 as raw little-endian `float64` IRE samples with a descriptor
(rate, time of sample 0, frame count).

## Open items

None.
