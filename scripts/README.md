# scripts

Analysis tools, independent verification and the round trip (TESTING.md,
"Independent verification" and "Round-trip verification"). Scripts run
inside the Nix dev shell, use the pinned `videosynth` and `ld-decode` and the
built `ld-creator`, and write all generated media beneath the build
directory, never into the source tree.

Run them with `python3 -E -B`. The pinned ld-decode package exports a
`PYTHONPATH` for its own Python 3.12 into the dev shell, and without `-E` the
analysis interpreter would load that environment's NumPy and fail. `-B`
keeps Python's bytecode cache out of the source tree.

| File | Contents |
|------|----------|
| `rf_analysis.py` | The tools: tap reader, `.s16` reader, analytic-signal instantaneous frequency, phase-slope frequency over a window, sinusoid and harmonic fits, sub-sample level and phase crossings, Welch spectra, transfer functions and group delay, occupied band, the readout transfer H(ν) and the S8 disc geometry |
| `test_rf_analysis.py` | Self-tests on synthetic signals (plan Task 6.1) |
| `verify_stages.py` | Stage-by-stage checks and the RF occupancy survey (plan Tasks 6.2 and 6.3) |
| `projects/` | The videosynth projects `verify_stages.py` runs |
| `cvbs_compare.py` | The round-trip comparator: CVBS reader, IEC 60857 §10 VBI decoders, frame alignment and the measurements of `docs-tech/user/ntsc-round-trip-metrics.md` |
| `test_cvbs_compare.py` | Self-tests of the comparator on synthetic lines |
| `round_trip.py` | The round-trip runner: videosynth, ld-creator, ld-decode, the reference demodulator and the comparator (plan Tasks 7.2 and 7.4) |
| `reference_demod.py` | A reference NTSC demodulator written from IEC 60857 alone: FM demodulation, de-emphasis, the §9.1.7 playback equaliser and the inverse readout transfer; writes CVBS the comparator scores |
| `test_reference_demod.py` | Self-tests of the reference demodulator on synthetic FM |
| `vits_measure.py` | Absolute VITS measurements without a truth file: burst level and timing, VIRS, the NTC-7 composite and combination lines, found by content. Measures a decoded `.cvbs`, or raw RF through a line-locked IEC 60857 demodulation with an optional FM sideband fit; used on real captures |
| `standard_burst.py` | Copies an NTSC truth with every colour burst replaced by the BT.470-6 burst (5.3 µs, 9 cycles), for the burst bisection of `docs-tech/user/ld-decode-findings.md`. The pinned videosynth's burst is already standard, so it matters only for truths from earlier videosynth versions |
| `test_vits_measure.py` | Self-tests of both on synthetic lines and synthetic FM |
| `capture_survey.py` | Cuts, decodes and measures the same excerpt of several captures (and ld-creator RF) and tabulates them side by side, for the multi-disc comparison of `docs-tech/user/ld-decode-findings.md` ("Other discs") |
| `fit_ddd_filter.py` | Fits the DomesdayDuplicator anti-alias filter's effective component values to the measured board (register S13.filter_fit); prints the values the model uses and the residuals |

## Independence

The tools are written from the stage notes' equations and first principles.
They import nothing from ld-creator and nothing from ld-decode, so a check
built on them does not share the implementation's errors. The comparator
decodes the VBI itself, from IEC 60857 §10, and uses nothing from ld-decode
but its output files. For example, the
readout transfer is integrated by adaptive quadrature rather than the
implementation's Gauss–Legendre rule, and the S7 reference edges come from
φ alone rather than from the Hermite interpolant on φ and f.

## Self-tests

```bash
nix develop "git+file://$PWD" --command python3 -E -B -m unittest discover -s scripts -p 'test_*.py'
```

Acceptance (plan Task 6.1): a synthetic tone, quantised as the Domesday
Duplicator would quantise it, is recovered to within 1 kHz, and synthetic
edges to within 0.1 ns. The phase-crossing test is tighter, at 0.1 ps,
because S7's check uses it against a 4 ps budget.

## Stage checks and occupancy survey

```bash
nix develop "git+file://$PWD" --command cmake --build build
nix develop "git+file://$PWD" --command python3 -E -B scripts/verify_stages.py
```

Options:

- `--build-dir <dir>`: the build tree. The default is `build`; the script
  uses `<dir>/ld-creator` and writes to `<dir>/verification/`.
- `--skip-patterns`: run only the stage checks (Task 6.2).

The script does the following:

1. It runs videosynth on `projects/vits_videosynth.yaml`: one frame of a
   photograph, with NTC-7 and FCC test lines.
2. It runs that frame through ld-creator at CAV 55, 80, 110 and 144.99 mm
   and CLV 10.1 and 11.4 m/s, with the taps each check needs. It writes
   each run's project as JSON, which is valid YAML.
3. It checks:
   - S3 and S5;
   - S4 against its analytic transfer function;
   - S7 edges against φ;
   - S8 track positions against the note's formulas;
   - S11 modulation depth and demodulated video gain against the note.
4. It runs videosynth on `projects/patterns_videosynth.yaml`, and each
   legal-range pattern (100 % bars and the full-range ramp are left out)
   through ld-creator three times: at 55 mm and 144.99 mm with the
   DdD profile, and at 144.99 mm with the ideal profile. It records each
   pattern's frequency range, occupied band and residual above 20 MHz.

The results go to `<dir>/verification/results.md` and `results.json`, and
the tool commands and their output to `runs.log`. The exit status is 1 if
any check fails. A full run takes about a minute.

The recorded results are in the stage notes' Verification sections
(`docs-tech/user/disc-model/s03…s11`) and in
`docs-tech/user/disc-model/rf-occupancy.md`.

## Round trip

```bash
nix develop "git+file://$PWD" --command cmake --build build
nix develop "git+file://$PWD" --command python3 -E -B scripts/round_trip.py
```

Options:

- `--build-dir <dir>`: the build tree (default `build`); the script uses
  `<dir>/ld-creator`.
- `--output-dir <dir>`: where to write (default `<build-dir>/round_trip`).
- `--baselines <name>…`: the baselines to run. The default is the plan's
  four: `carrier`, `cav_inner`, `cav_outer` and `clv`. `cav_inner_equalised`
  is the MTF bisection run of the metrics document, run by name.
- `--no-reference`: skip the reference demodulator.
- `--threads <n>`: ld-creator's thread count; the RF does not depend on it.

The script does the following:

1. It logs the tool versions: the ld-creator commit and build, and the
   videosynth and ld-decode versions with their `flake.lock` pins.
2. It runs videosynth on
   `tests/functional/projects/ntsc_round_trip_videosynth.yaml`: 100 NTSC CAV
   frames with LaserVision VITS and VBI codes.
3. It checks the comparator against itself: the truth against itself must
   be confirmed, and the truth against the truth + 2 IRE refuted.
4. For each baseline, it runs ld-creator on
   `tests/functional/projects/ntsc_round_trip_<baseline>.yaml`, decodes the RF
   with `ld-decode --NTSC` (adding `-m 0` for RF with no readout MTF), and
   compares the decoded CVBS with the truth.
5. It decodes the same RF with `reference_demod.py` (with the playback
   equaliser, and for disc RF the inverse readout transfer) and compares
   that too. If the reference decoder recovers the truth, the RF carries the
   IEC 60857 signal. It does not compensate the DdD capture filter, so on
   DdD disc RF it shows that filter's effect.

The results go to `<output-dir>/results.md` and `results.json`, and the tool
commands, their output and their run times to `runs.log`. The exit status is
0 when every judged prediction is confirmed, 1 when one is refuted, and 2
when a tool or the comparator's own check fails; the exit status follows
ld-decode's verdicts only. The four baselines take about eight minutes.

The comparator also runs on its own:

```bash
python3 -E -B scripts/cvbs_compare.py <truth.cvbs> <decoded.cvbs> [--json out.json]
```

and so does the reference demodulator:

```bash
python3 -E -B scripts/reference_demod.py <rf.s16> <truth.cvbs> <out.cvbs> \
    [--playback-equaliser] [--mtf <resolved project>]
```

The recorded results are in `docs-tech/user/ntsc-round-trip-metrics.md`
("Results"), and the discrepancies in `docs-tech/user/ld-decode-findings.md`.

## Real captures

A real capture has no truth, so it is measured against the test signals'
nominal values. `vits_measure.py` finds the NTC-7 composite, the NTC-7
combination and VIRS by their content, because discs carry them on
different lines.

Cut an excerpt from a DomesdayDuplicator capture with ld-cut, which writes
raw 16-bit RF at 40 MSPS (the format ld-creator writes), into the build
directory:

```bash
nix develop "git+file://$PWD" --command ld-cut -n -s <file frame> -l <frames> \
    <capture.ldf> build/<dir>/<excerpt>.s16
```

Then decode it with ld-decode as the round trip does, and measure the decode,
or the RF itself:

```bash
python3 -E -B scripts/vits_measure.py cvbs <decoded.cvbs> [--frames N]
python3 -E -B scripts/vits_measure.py rf <rf.s16> [--frames N] \
    [--playback-equaliser] [--sidebands] [--rf-band LOW HIGH]
```

The RF mode demodulates as `reference_demod.py` does (IEC 60857 carrier
frequencies, de-emphasis, optionally the §9.1.7 playback equaliser), but
with no readout or capture compensation, and resamples each line to 4fsc
from its own sync. Its default RF band starts at 4 MHz, above the analogue
audio carriers a real disc carries. On ld-creator's carrier diagnostic it
reproduces the truth within 0.4 % in chroma and 0.03 dB in the multiburst.
`--sidebands` fits the burst's and each multiburst packet's first and second
FM sidebands relative to the carrier.

Running the same measurements on ld-creator RF at an excerpt's radius
shows where ld-creator's RF and the real disc differ, independently of
ld-decode. The recorded comparison is in
`docs-tech/user/ld-decode-findings.md` ("The real capture").

To compare several captures at once:

```bash
nix develop "git+file://$PWD" --command python3 -E -B scripts/capture_survey.py \
    --output-dir build/captures --start 1000 --frames 40 \
    "<label>=<capture.ldf or .raw.oga or .s16>" ...
```

It cuts each capture (an `.s16` is used as it is), decodes it with
`ld-decode --NTSC` and with `--no_auto_chroma`, measures both decodes and
the RF, reads ld-decode's calibration and lock from its log, and writes one
column per source to `<output-dir>/survey.md`. Steps whose output exists
are skipped. The recorded survey is in the findings document ("Other
discs").

The burst bisection:

```bash
python3 -E -B scripts/standard_burst.py build/round_trip/round_trip.cvbs \
    build/<dir>/round_trip.cvbs
```

writes a copy of the truth whose bursts start at 5.3 µs and last 9 cycles at
their 50 % points, with the original amplitude and phase and 300 ns edges.
Running ld-creator's round-trip projects with `--output-root build/<dir>`
then uses it.
