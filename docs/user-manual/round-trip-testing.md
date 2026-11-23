# Round-trip Testing

!!! warning "Planned"
    This page describes the intended workflow. The round-trip harness is not yet implemented.

Round-trip testing measures how accurately ld-decode recovers a LaserDisc signal by starting from a signal whose every sample is known:

```
videosynth ──CVBS/WAV/EFM──► ld-creator ──RF──► ld-decode ──CVBS/WAV/EFM──► compare
     │                                                                           ▲
     └───────────────────────────── original (truth) ────────────────────────────┘
```

1. **videosynth** generates the original: composite video, analogue audio and, optionally, EFM digital audio, with laserdisc VBI codes.
2. **ld-creator** modulates it into the RF a player would read from a disc.
3. **ld-decode** decodes that RF.
4. The decoded output is compared with the original — video line by line, VBI codes, audio and EFM.

## Pinned tool versions

The ld-creator development shell contains the exact versions of videosynth and ld-decode that it is tested against. Using those versions means a result can be reproduced later from the same ld-creator commit. See the repository's `BUILDING.md` for how the versions are pinned and updated.

## Interpreting differences

ld-creator follows the LaserVision standards rather than any particular decoder. When the decoded output differs from the original:

- If ld-creator's RF matches the standard, the difference is a finding about the decoder.
- Where the standard allows a range (for example the ±50 kHz tolerance on the carrier frequency), ld-creator records the value it used so the choice can be reviewed.
