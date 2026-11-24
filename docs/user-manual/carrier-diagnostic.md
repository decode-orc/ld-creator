# Carrier Diagnostic

A **carrier diagnostic** writes the frequency-modulated video carrier directly as RF, without any of the stages that model a disc. It is a fault-finding tool, **not a disc**: the RF has no readout loss, no capture filter and no pits.

Use it to split a problem in two. If ld-decode recovers the video from the carrier diagnostic but not from a modelled disc, the fault lies in the disc stages (the cut, the readout or the capture). If it fails on the carrier too, the fault lies in the video signal processing or in the decoder.

## Running one

Set `output.mode` in the project:

```yaml
input:
  video_path: "{output}/ntsc_cav.cvbs"

disc_cutter:
  format: cav
  start_radius_mm: 55.0
  track_pitch_um: 1.6667

output:
  rf_path: "{output}/ntsc_cav_carrier.s16"
  mode: carrier_diagnostic
```

```bash
ld-creator --project carrier.yaml
```

`disc_cutter` is still required and checked, so the same project can be switched to `mode: disc` later.

## What it contains

The video goes through the signal-processing stages of IEC 60857 §9, with the settings in the project:

1. **Rate conversion** (`video_rate_converter`): from 4fsc to the modelling rate, 24 fsc ≈ 85.909 MHz.
2. **Group-delay pre-distortion** (`group_delay_predistorter`, §9.1.7).
3. **Pre-emphasis** (`video_pre_emphasis`, §9.2.4).
4. **Frequency modulation** (`fm_phase_modulator`, §9.2.1–9.2.3): 8.1 MHz at blanking, 1.7 MHz deviation from sync tip to white.

The carrier, cos φ, is then sampled at 40 MHz and quantised with the [`rf_capture`](../reference/project-file.md#rf_capture) profile, so that its peak-to-peak amplitude is `level_fraction` of the converter's full scale. The file format is in [RF output](../reference/rf-output.md).

## What it records

- The RF file named by `output.rf_path`.
- The [resolved project](../reference/project-file.md#the-resolved-project) beside it. Its header says that the file is not a disc, and its `output.mode` is `carrier_diagnostic`. Running the resolved project gives the same RF, byte for byte.
- Any [diagnostic taps](../reference/diagnostic-taps.md) on the stages above, and on `cvbs_input`. Taps on later stages are an error in this mode.

The output does not depend on `--threads`.
