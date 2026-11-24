# Quick Start

!!! warning "Partly implemented"
    ld-creator writes the RF of an ideal NTSC video disc. The comparison of ld-decode's output with the original is not implemented yet, so this page stops at the RF file.

## What works today

After [building](installation.md), ld-creator checks a [project file](../reference/project-file.md) and the videosynth recording it names, and turns the recording into RF:

```bash
./build/ld-creator --version
./build/ld-creator --project disc.yaml --validate
./build/ld-creator --project disc.yaml
```

A minimal project for an ideal NTSC CAV disc:

```yaml
input:
  video_path: "{output}/ntsc_cav.cvbs"

disc_cutter:
  format: cav
  start_radius_mm: 55.0
  track_pitch_um: 1.6667

output:
  rf_path: "{output}/ntsc_cav.s16"
```

`--validate` reports every problem with the project or its input and exits with status 0 if there are none. Running without `--validate` performs the same checks and then models the disc: it writes the RF to `rf_path` (signed 16-bit samples at 40 MHz, as a Domesday Duplicator capture) and a resolved project beside it. A run takes about a second per frame on a multi-core machine; see [Disc model](../user-manual/disc-model.md).

The RF is read by ld-decode as it is, with its defaults:

```bash
ld-decode --NTSC ntsc_cav.s16 ntsc_cav
```

For fault-finding, `mode: carrier_diagnostic` in the `output` block writes the bare FM carrier instead; see [Carrier diagnostic](../user-manual/carrier-diagnostic.md).

## What the walkthrough will cover

1. Generate a short NTSC test signal with videosynth.
2. Modulate it to RF with ld-creator.
3. Decode the RF with ld-decode.
4. Compare the decoded output with the original.

See [Round-trip testing](../user-manual/round-trip-testing.md) for the design of this workflow.
