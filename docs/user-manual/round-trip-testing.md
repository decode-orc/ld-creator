# Round-trip Testing

Round-trip testing measures how accurately ld-decode recovers a LaserDisc signal by starting from a signal whose every sample is known:

```
videosynth ──CVBS──► ld-creator ──RF──► ld-decode ──CVBS──► compare
     │                                                         ▲
     └────────────────── original (truth) ─────────────────────┘
```

1. **videosynth** generates the original: NTSC composite video with LaserVision VITS and VBI codes.
2. **ld-creator** modulates it into the RF a player would read from a disc.
3. **ld-decode** decodes that RF.
4. A comparator measures the decoded video against the original: levels, chroma, frequency response, pulse response, timing and the VBI codes.

The same RF is also decoded by a reference demodulator written from the LaserVision standard alone. If it recovers the original, the RF itself is right by the standard.

The round trip covers NTSC video. Audio and EFM are planned.

## Running it

The round trip runs in the development shell, which holds the pinned videosynth and ld-decode and the analysis Python. Build ld-creator first, then run the script from the repository root:

```bash
nix develop "git+file://$PWD" --command cmake --build build
nix develop "git+file://$PWD" --command python3 -E -B scripts/round_trip.py
```

It writes everything into `build/round_trip/`:

| File | Contents |
|------|----------|
| `results.md` | The tool versions and, for each baseline, a table for ld-decode and one for the reference demodulator: each measurement's minimum, mean and maximum, the frames outside its threshold, and the verdict |
| `results.json` | The same, with every individual value |
| `runs.log` | Every command run, its output and its run time |
| `round_trip.cvbs`, `<baseline>.s16`, `<baseline>_decoded.cvbs`, `<baseline>_reference.cvbs` | The truth, the RF, and the video decoded by ld-decode and by the reference demodulator |

The exit status is 0 when every measurement ld-decode is judged on is confirmed, 1 when one is refuted, and 2 when a tool fails.

## Baselines

The repository's `tests/functional/projects/` holds one videosynth project and one ld-creator project per baseline. The ld-creator projects differ only in their stage blocks:

| Baseline | ld-creator project | ld-decode options |
|----------|--------------------|-------------------|
| Carrier diagnostic (no disc) | `ntsc_round_trip_carrier.yaml` | `--NTSC -m 0` |
| Ideal disc, CAV, 55 mm | `ntsc_round_trip_cav_inner.yaml` | `--NTSC` |
| Ideal disc, CAV, ending at 145 mm | `ntsc_round_trip_cav_outer.yaml` | `--NTSC` |
| Ideal disc, CLV, 10.1 m/s | `ntsc_round_trip_clv.yaml` | `--NTSC` |

Run a subset with `--baselines`, for example `--baselines carrier cav_inner`.

## Pinned tool versions

The ld-creator development shell contains the exact versions of videosynth and ld-decode that it is tested against. Using those versions means a result can be reproduced later from the same ld-creator commit. `results.md` records the versions used. See the repository's `BUILDING.md` for how the versions are pinned and updated.

## Interpreting differences

ld-creator follows the LaserVision standards rather than any particular decoder. When the decoded output differs from the original:

- If ld-creator's RF does not match the standard, ld-creator is fixed.
- If it does, the difference is not yet a finding about the decoder. ld-decode is calibrated on real discs, and a real disc can depart from the standard's nominal values. A difference is attributed to the decoder only when a real capture shows that ld-creator's RF resembles a real disc where the difference arises; until then it is recorded as unattributed.
- If the original itself departs from the standard, the difference belongs to the original, not to ld-creator or the decoder. The pinned videosynth's colour burst, for example, is about 7.3 cycles long where the standard calls for 9 ± 1; ld-decode reads its burst low and so decodes all chroma about 10 % high.
- A real capture is compared without a truth file: `scripts/vits_measure.py` measures the test signals a disc carries against their nominal values, in a decode or in the RF itself (see the repository's `scripts/README.md`, "Real captures"). One disc shows how ld-creator's RF compares with that disc; mastering differed between discs, so it does not define real discs in general.
- Where the standard allows a range (for example the ±50 kHz tolerance on the carrier frequency), ld-creator records the value it used so the choice can be reviewed.

The carrier diagnostic narrows a difference down: its RF holds no disc stages, so a measurement that fails there arises in the FM signal or the decoder's FM and video path, not in the disc model. A reference demodulator written from the standard (`scripts/reference_demod.py`) checks that the RF itself decodes to the original. The current results and their classification are recorded in the repository's technical documents (`docs-tech/user/ntsc-round-trip-metrics.md` and `docs-tech/user/ld-decode-findings.md`).
