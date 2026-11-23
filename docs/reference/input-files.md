# Input Files

!!! warning "Planned"
    ld-creator does not read input files yet. This page describes the inputs it is designed to accept.

ld-creator reads files laid out by the [CVBS file format specification](https://github.com/decode-orc/cvbs-file-format-specification){target="_blank"}, as written by videosynth. All files share a base name.

| File | Required | Contents |
|------|----------|----------|
| `<base>.cvbs` | Yes | Composite video samples |
| `<base>.meta` | Yes | SQLite metadata: video standard (PAL or NTSC), sample encoding, level mapping |
| `<base>_audio_<pair>.wav` | No | 48 kHz / 24-bit stereo audio. Pair 1 becomes analogue channels I (left) and II (right) |
| `<base>.efm` | No | EFM t-values (T3–T11) for the digital audio channel |
| `<base>.efm.meta` | With `.efm` | SQLite index aligning the EFM stream to video frames |

## Constraints

- **Composite only.** Y/C input (`.cvbsy` / `.cvbsc`) is not accepted; a LaserDisc records a composite signal.
- **PAL and NTSC only.** These are the systems defined by IEC 60856 and IEC 60857.
- The video standard is read from `<base>.meta` and cannot be overridden.
