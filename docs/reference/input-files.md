# Input Files

ld-creator reads files laid out by the [CVBS file format specification](https://github.com/decode-orc/cvbs-file-format-specification){target="_blank"}, as written by videosynth. All files share a base name, and the project's [`input.video_path`](project-file.md#input) names the `.cvbs`.

| File | Required | Contents | Status |
|------|----------|----------|--------|
| `<base>.cvbs` | Yes | Composite video samples | Read |
| `<base>.meta` | Yes | SQLite metadata: video standard, sample encoding, signal state, frame count | Read |
| `<base>_audio_<pair>.wav` | No | 48 kHz / 24-bit stereo audio. Pair 1 becomes analogue channels I (left) and II (right) | Planned |
| `<base>.efm` | No | EFM t-values (T3–T11) for the digital audio channel | Planned |
| `<base>.efm.meta` | With `.efm` | SQLite index aligning the EFM stream to video frames | Planned |

## Video

The `cvbs_file` row of the `.meta` must declare:

| Field | Accepted values | Why |
|-------|-----------------|-----|
| `preset` | `NTSC` | IEC 60857 (NTSC) is the first standard implemented |
| `signal_type` | `composite` | A LaserVision disc records composite video; Y/C input is not accepted |
| `signal_state_preset` | `STANDARD_STABLE_LOCKED`, `STANDARD_STABLE_UNLOCKED` | The samples must be at 4fsc and time-base stable, so every line is 910 samples and the stored line origin holds |
| `sample_encoding_preset` | `CVBS_U10_4FSC`, `CVBS_U16_4FSC`, `CVBS_S16_4FSC`, `CVBS_TPG21_4FSC` | The 4fsc encodings with a defined level mapping |

The `.cvbs` must hold whole frames of 477 750 samples (955 500 bytes). A partial final frame is an error naming the frame; so is a frame count that disagrees with `number_of_sequential_frames`, when the metadata gives one.

### Conversion to IRE

Each stored word is decoded to the 10-bit value *v* as its encoding defines, then converted to IRE with SMPTE 244M-2003 §4.2.1 levels (blanking 240, white 800):

IRE = 100 · (*v* − 240) / 560

So sync tip (16) is −40 IRE, black (282) +7.5 IRE and white (800) 100 IRE. Values outside 0–1023, such as the LaserVision VBI burst below sync tip that `CVBS_S16_4FSC` can carry, keep their value and sign; they are not clamped. The `.meta` `black_level` override does not affect the conversion, because black is a picture level, not a reference of the IRE scale.

### Timing

Sample 0 of each stored line is the first sampling instant at or after that line's 0H (the CVBS specification's stored-line alignment), so 0H falls 57/90 of a sample period before it. ld-creator's time origin is 0H of frame line 1 of the first frame read.
