# scripts

Analysis tools and independent verification (TESTING.md, "Independent
verification"). Scripts run inside the Nix dev shell, use the pinned
`videosynth` and the built `ld-creator`, and write all generated media
beneath the build directory, never into the source tree.

Run them with `python3 -E`. The pinned ld-decode package exports a
`PYTHONPATH` for its own Python 3.12 into the dev shell, and without `-E` the
analysis interpreter would load that environment's NumPy and fail.

| File | Contents |
|------|----------|
| `rf_analysis.py` | The tools: tap reader, `.s16` reader, analytic-signal instantaneous frequency, phase-slope frequency over a window, sinusoid and harmonic fits, sub-sample level and phase crossings, Welch spectra, transfer functions and group delay, occupied band, the readout transfer H(ν) and the S8 disc geometry |
| `test_rf_analysis.py` | Self-tests on synthetic signals (plan Task 6.1) |
| `verify_stages.py` | Stage-by-stage checks and the RF occupancy survey (plan Tasks 6.2 and 6.3) |
| `projects/` | The videosynth projects `verify_stages.py` runs |

## Independence

The tools are written from the stage notes' equations and first principles.
They import nothing from ld-creator and nothing from ld-decode, so a check
built on them does not share the implementation's errors. For example, the
readout transfer is integrated by adaptive quadrature rather than the
implementation's Gauss–Legendre rule, and the S7 reference edges come from
φ alone rather than from the Hermite interpolant on φ and f.

## Self-tests

```bash
nix develop "path:$PWD" --command python3 -E -m unittest discover -s scripts -p 'test_*.py'
```

Acceptance (plan Task 6.1): a synthetic tone, quantised as the Domesday
Duplicator would quantise it, is recovered to within 1 kHz, and synthetic
edges to within 0.1 ns. The phase-crossing test is tighter, at 0.1 ps,
because S7's check uses it against a 4 ps budget.

## Stage checks and occupancy survey

```bash
nix develop "path:$PWD" --command cmake --build build
nix develop "path:$PWD" --command python3 -E scripts/verify_stages.py
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
   pattern through ld-creator three times: at 55 mm and 144.99 mm with the
   DdD profile, and at 144.99 mm with the ideal profile. It records each
   pattern's frequency range, occupied band and residual above 20 MHz.

The results go to `<dir>/verification/results.md` and `results.json`, and
the tool commands and their output to `runs.log`. The exit status is 1 if
any check fails. A full run takes about a minute.

The recorded results are in the stage notes' Verification sections
(`docs-tech/user/disc-model/s03…s11`) and in
`docs-tech/user/disc-model/rf-occupancy.md`.
