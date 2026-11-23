# Quick Start

!!! warning "Planned"
    RF modulation is not yet implemented, so there is no end-to-end walkthrough yet. This page will take you from a videosynth project to an ld-decode result once the pipeline is available.

## What works today

After [building](installation.md), the command-line tool reports its version and usage:

```bash
./build/ld-creator --version
./build/ld-creator --help
```

Running it without `--help` or `--version` currently logs that modulation is not implemented and exits with a non-zero status.

## What the walkthrough will cover

1. Generate a short PAL test signal with videosynth.
2. Modulate it to RF with ld-creator.
3. Decode the RF with ld-decode.
4. Compare the decoded output with the original.

See [Round-trip testing](../user-manual/round-trip-testing.md) for the design of this workflow.
