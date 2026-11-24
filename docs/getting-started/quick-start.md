# Quick Start

!!! warning "Planned"
    RF modulation is not yet implemented, so there is no end-to-end walkthrough yet. This page will take you from a videosynth project to an ld-decode result once the pipeline is available.

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

`--validate` reports every problem with the project or its input and exits with status 0 if there are none. Running without `--validate` performs the same checks, then logs that modulation is not implemented and exits with a non-zero status.

## What the walkthrough will cover

1. Generate a short NTSC test signal with videosynth.
2. Modulate it to RF with ld-creator.
3. Decode the RF with ld-decode.
4. Compare the decoded output with the original.

See [Round-trip testing](../user-manual/round-trip-testing.md) for the design of this workflow.
