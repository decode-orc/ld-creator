# ld-creator High-Level Design

Living design specification for ld-creator. Keep it aligned with the code
(AGENTS.md §9.5). Each section states whether it is **implemented** or
**designed**; designed sections describe intent, not available behaviour.

---

## 1. Purpose

ld-creator converts a CVBS recording and its associated media into the RF
signal a LaserVision player would recover from a disc mastered to
IEC 60856 (PAL) or IEC 60857 (NTSC).

It is one stage of a known-truth measurement loop for ld-decode:

```
            truth                          RF                        decoded
videosynth ───────► ld-creator ─────────────────────► ld-decode ───────────► compare
 (CVBS, WAV, EFM)   (this repo)   (.s16 / .lds / .ldf)               (CVBS, WAV, EFM)
     │                                                                          ▲
     └──────────────────────────────── truth ───────────────────────────────────┘
```

Because videosynth's output is exact and ld-creator follows the standards,
any difference between the truth and ld-decode's output is attributable to the
decode chain (or, where the standard allows latitude, to a documented
ld-creator modelling choice).

### 1.1 Non-goals

- Modelling a specific player's optics, servo or tracking behaviour beyond the
  measurement conditions of IEC 60856/60857 §11.
- Disc-surface impairments (dropouts, noise). Those are injected upstream by
  videosynth into the CVBS so that the truth already contains them; a later
  RF-domain impairment stage may be added as a separate, optional stage.
- Decoding. ld-creator never demodulates RF.

---

## 2. External interfaces

### 2.1 Inputs — *designed*

All inputs follow the CVBS file format specification
(`docs-tech/cvbs-file-format-specification/docs/index.md`).

| Input | File(s) | Source spec |
|-------|---------|-------------|
| Composite video | `<base>.cvbs` + `<base>.meta` (SQLite) | CVBS spec, "Video Data Format", "Metadata Schema" |
| Analogue audio | `<base>_audio_<pair>.wav` (48 kHz / 24-bit stereo), `audio_channel_pair` table | CVBS spec, "Audio Data" |
| EFM digital audio | `<base>.efm` (T3–T11 t-value stream) + `<base>.efm.meta` | CVBS spec, `extensions/efm-extension-format.md` |

- The video standard (PAL / NTSC) is taken from the `.meta` video standard
  preset; ld-creator does not accept a conflicting override.
- Y/C (`.cvbsy`/`.cvbsc`) input is out of scope: a LaserDisc records composite.
- The EFM extension's t-value stream is the natural input to the EFM pulse
  generator — no CIRC or EFM *encoding* is needed in ld-creator; videosynth
  already produces the channel-bit-level t-values.
- Audio channel pair 1 maps to analogue channels I (left) and II (right).

### 2.2 Output — *designed*

A single real-valued RF sample stream in a format ld-decode reads natively
(ld-decode `lddecode/fileio.py`):

| Format | Description | Notes |
|--------|-------------|-------|
| `.s16` | Signed 16-bit little-endian, mono | First target: simplest, lossless |
| `.lds` | 10-bit packed | DomesdayDuplicator capture format |
| `.ldf` | FLAC-compressed `.lds`-domain samples | Large-file convenience |

- Default sample rate: **40 MHz**, ld-decode's default `--inputfreq`. Other
  rates are possible but require the matching `--inputfreq` at decode time.
- A sidecar (format TBD) records the generation parameters (standard, rate,
  carrier calibration, seed, ld-creator version, input hashes) so a round trip
  is reproducible from its outputs alone.

### 2.3 Command line — *implemented (skeleton only)*

`src/main.cpp`, `src/cli/command_line.cpp`. Implemented options: `--help`,
`--version`, `--log-level`, `--log-file`. Input/output options arrive with the
pipeline. User reference: `docs/reference/cli-options.md`.

---

## 3. Signal model — *designed*

The recorded signal is a **frequency-modulated main carrier** (video), onto
which the **audio subcarriers** and the **EFM signal** are added by
**symmetrical double-edge pulse-width modulation** of the main carrier — i.e.
they shift the zero crossings of the limited FM carrier rather than being
summed linearly into the RF.

### 3.1 Video path

```
CVBS samples → level normalisation → group-delay pre-distortion → video pre-emphasis → FM modulator
```

| Parameter | PAL (IEC 60856) | NTSC (IEC 60857) |
|-----------|-----------------|------------------|
| Modulation | FM, positive (§9.2.1) | FM, positive (§9.2.1) |
| Blanking-level carrier | 7.1 MHz ± 50 kHz (§9.2.2) | 8.1 MHz ± 50 kHz (§9.2.2) |
| Deviation | Blanking → +100 % white: 800 ± 20 kHz (§9.2.3) | Sync tip (−40 IRE) → white (+100 IRE): 1.7 MHz ± 35 kHz (§9.2.3) |
| Pre-emphasis | §9.2.4, Figure 12 | §9.2.4, Figure 9 |
| Group-delay pre-distortion | §9.1.6, Figure 11 (0.5–4.8 MHz) | §9.1.6 (0.5–4.2 MHz) |
| Pilot burst | 3.75 MHz (240 fH) on sync level (§9.1.2) — present in the CVBS from videosynth | — |

- Level normalisation uses the sample-encoding preset's sync/blanking/white
  mapping from the CVBS `.meta`; nothing is assumed about the integer range.
- The modulator integrates instantaneous frequency to phase at the output
  rate (or an oversampled rate followed by a decimating filter) with
  double-precision phase accumulation, so carrier phase is deterministic.

### 3.2 Analogue audio path

```
WAV (48 kHz) → [optional CX compression, §8.4/App. B] → pre-emphasis (§8.3) → resample → FM subcarrier → PWM onto main carrier
```

| Parameter | PAL (IEC 60856 §8) | NTSC (IEC 60857 §8) |
|-----------|--------------------|---------------------|
| Channel I | 43.75 fH = 683 593.75 Hz | 146.25 fH = 2 301 136 Hz |
| Channel II | 68.25 fH = 1 066 406.25 Hz | 178.75 fH = 2 812 499 Hz |
| Deviation | ±100 kHz at 100 %, peak < ±150 kHz (§8.1) | same |
| Subcarrier level | −26 ± 1 dB re unmodulated main carrier (§7.2.2) | same |

- Subcarrier frequencies are derived from fH (line-locked), not rounded
  constants.

### 3.3 EFM path

```
.efm t-values → NRZ channel bits at 4.3218 Mbit/s → low-pass + high-pass + pre-emphasis (Amendment 2 §13.1) → PWM onto main carrier
```

- Filters per IEC 60856/60857 Amendment 2 §13.1.2 (low-pass −3 dB at
  1.75 MHz, < −50 dB above 2.3 MHz, with specified group-delay
  pre-distortion) and Figure 24 (high-pass, pre-emphasis).
- The EFM channel occupies the spectrum below ~1.75 MHz, which in PAL overlaps
  the analogue audio subcarriers; a PAL disc carries either analogue or digital
  audio on that band. The PAL rule must be confirmed against Amendment 2 before
  implementation — **open question**.
- Channel-bit clock relationship to video: the EFM stream is not line-locked;
  the frame index in `.efm.meta` aligns it to video frames.

### 3.4 Pulse-width combination

The final RF is a limited (square-ish) FM carrier whose duty cycle is
modulated by the sum of the audio subcarriers and the EFM signal. The
implementation models this as slicing the FM carrier's phase against a
threshold offset by the PWM signal, then band-limiting to the player's
nominal RF bandwidth before quantisation. The exact band-limiting model
(and whether to model the optical MTF of §11) is an **open question**
affecting what ld-decode is measured against.

---

## 4. Module layout

Implemented modules are marked ✔; the rest are planned.

| Module | Directory | Responsibility |
|--------|-----------|----------------|
| ✔ cli | `src/cli/`, `include/ld_creator/cli/` | Command-line parsing and usage text |
| cvbs_input | `src/cvbs_input/` | Read `.cvbs` fields and `.meta`; expose level mapping |
| audio_input | `src/audio_input/` | Read WAV channel pairs |
| efm_input | `src/efm_input/` | Read `.efm` t-values and `.efm.meta` frame index |
| dsp | `src/dsp/` | Shared FIR/IIR filters, resamplers, NCO |
| video_modulation | `src/video_modulation/` | §3.1 |
| audio_modulation | `src/audio_modulation/` | §3.2 |
| efm_modulation | `src/efm_modulation/` | §3.3 |
| rf_combiner | `src/rf_combiner/` | §3.4 |
| rf_output | `src/rf_output/` | `.s16` / `.lds` / `.ldf` writers and parameter sidecar |
| pipeline | `src/pipeline/` | Field-by-field orchestration, threading, determinism |

All modules are built into the `ld_creator_core` library; `src/main.cpp` is
the only source outside it. Modules depend on interfaces (`I`-prefixed) and
receive dependencies by constructor injection (TESTING.md).

---

## 5. Round-trip harness — *designed*

Functional tests and `scripts/` drive the loop in §1 with the pinned tools:

1. Run the pinned `videosynth` on a committed project YAML into the build tree.
2. Run `ld-creator` on its output.
3. Run the pinned `ld-decode` on the RF.
4. Compare decoded output against the truth: per-line video error (after
   aligning on the CVBS frame/line origin), VBI codes, audio SNR, EFM
   t-value / sector error counts.

Metrics and pass thresholds are to be defined per test and recorded in
`docs-tech/user/` before implementation.

---

## 6. Open questions

- PAL analogue/digital audio coexistence on the low band (§3.3).
- RF band-limiting and optical MTF model (§3.4).
- Whether ld-creator should accept videosynth's `RAW_S16_*` high-rate presets
  directly, avoiding a 4fsc → RF-rate resample.
- Parameter sidecar format (§2.2).
