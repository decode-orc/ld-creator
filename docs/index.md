# Home

Welcome to the ld-creator documentation.

## What is ld-creator?

**ld-creator** masters PAL and NTSC LaserDiscs in software. It takes a composite video recording in the [CVBS file format](https://github.com/decode-orc/cvbs-file-format-specification){target="_blank"}, together with its analogue audio and EFM digital audio, and produces the RF signal a LaserDisc player would read from a disc mastered to IEC 60856 (PAL) or IEC 60857 (NTSC).

Its main use is measuring decoder accuracy. Paired with its sister tools it closes a loop in which every sample of the original is known:

```
videosynth ──CVBS/WAV/EFM──► ld-creator ──RF──► ld-decode ──CVBS/WAV/EFM──► compare with the original
```

- [videosynth](https://github.com/decode-orc/videosynth){target="_blank"} generates standards-accurate test signals — the *truth*.
- ld-creator modulates that truth into LaserDisc RF.
- [ld-decode](https://github.com/happycube/ld-decode){target="_blank"} decodes the RF back to CVBS, audio and EFM.

Any difference between what went in and what came out is a measured error in the decode chain.

## Development status

!!! warning "Not yet functional"
    ld-creator is in early development. It writes RF from a modelled **ideal NTSC video disc**: the video path from the CVBS input to the capture device's samples (see [Disc model](user-manual/disc-model.md)). Audio, EFM, PAL and disc impairments are not implemented yet, and the RF has not yet been verified against ld-decode. Pages in this documentation say explicitly where a feature is planned rather than available.

## Where to start

- [Installation](getting-started/installation.md) — build ld-creator from source.
- [Quick Start](getting-started/quick-start.md) — first run.
- [Round-trip testing](user-manual/round-trip-testing.md) — how ld-creator fits between videosynth and ld-decode.
- [Reference](reference/cli-options.md) — command-line options, input files and the RF output format.

If you spot a mistake, or would like to report an issue or contribute, please use the main repository:

[ld-creator GitHub repository](https://github.com/decode-orc/ld-creator){target="_blank"}
