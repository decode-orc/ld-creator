# Quick Start

!!! warning "Planned"
    Disc modulation is not yet implemented, so there is no end-to-end walkthrough yet. This page will take you from a videosynth project to an ld-decode result once the pipeline is available.

## What works today

After [building](installation.md), ld-creator checks a [project file](../reference/project-file.md) and the videosynth recording it names:

```bash
./build/ld-creator --version
./build/ld-creator --project disc.yaml --validate
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

`--validate` reports every problem with the project or its input and exits with status 0 if there are none. Running without `--validate` performs the same checks; a disc project then stops, because disc modulation is not implemented yet.

To write RF today, add `mode: carrier_diagnostic` to the `output` block and run the project without `--validate`. ld-creator writes the FM-modulated video carrier to `rf_path`, with a resolved project beside it. That file is a fault-finding aid, not a disc; see [Carrier diagnostic](../user-manual/carrier-diagnostic.md).

## What the walkthrough will cover

1. Generate a short NTSC test signal with videosynth.
2. Modulate it to RF with ld-creator.
3. Decode the RF with ld-decode.
4. Compare the decoded output with the original.

See [Round-trip testing](../user-manual/round-trip-testing.md) for the design of this workflow.
