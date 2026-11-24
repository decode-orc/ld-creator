# Diagnostic Taps

A **tap** writes the signal between two stages to a file, so each stage can be inspected on its own. Taps are declared in the project's [`diagnostics`](project-file.md#diagnostics) block and never change the RF.

A disc run can write every tap below. A [carrier-diagnostic](../user-manual/carrier-diagnostic.md) run stops after `fm_phase_modulator`, so only the first five can be tapped there.

Each tap writes two files next to its base `path`: a data file and a YAML descriptor, `<path>.tap.yaml`. All binary data is little-endian.

## Which stages have a tap

| Stage | Representation | Kind | Contents |
|-------|----------------|------|----------|
| `cvbs_input` | D1 | sampled | Video in IRE at 4fsc |
| `video_rate_converter` | D2 | sampled | Video in IRE at the modelling rate Fm = 24 fsc ≈ 85.909 MHz |
| `group_delay_predistorter` | D2 | sampled | Video after group-delay pre-distortion |
| `video_pre_emphasis` | D2 | sampled | Video after pre-emphasis |
| `fm_phase_modulator` | D3 | sampled | Carrier phase φ in radians (one sample per modelling-rate period) |
| `recording_limiter` | D4 | edges | Recorded transition times |
| `disc_cutter` | D5 | edges | Pit edges on the disc |
| `optical_readout` | D8 | sampled | Photocurrent at 80 MHz, unpitted land = 1 |
| `player_front_end` | D9 | sampled | Player RF at 80 MHz, land = 1. With `mtf_equaliser: none` it equals the `optical_readout` tap |
| `rf_capture` | `D9 filtered` | sampled | The converter's input *u* at 80 MHz, before sampling: ±1 is the converter's range (unit `ADC half-scale`). The converter takes every second sample |

## Time origin

Time 0 is 0H (the half-amplitude point of the leading sync edge) of frame line 1 of the first input frame. Sample *n* of a sampled stream is at (*n* + origin) / rate, where the origin is 57/90 of a sample for D1 (the CVBS stored-line alignment of NTSC) and 0 for every other stream.

## Sampled taps

Data file `<path>.f64`: one IEEE 754 binary64 (`float64`) per sample, no header.

| Descriptor key | Meaning |
|----------------|---------|
| `format` | `ld-creator-tap` |
| `format_version` | `1` |
| `stage` | The stage module name |
| `representation` | `D1` … `D9`, or `D9 filtered` for `rf_capture` |
| `kind` | `sampled` |
| `data_file` | The data file's name |
| `unit` | Unit of the samples |
| `first_frame`, `frame_count` | The frames the tap covers, as declared |
| `sample_type` | `float64_le` |
| `rate_hz` | Sample rate |
| `first_sample` | Stream index of the first sample in the file |
| `time_of_first_sample_s` | Time of that sample, in seconds |
| `sample_count` | Number of samples in the file |

## Edge taps

Data file `<path>.edges`: packed fixed-size records, no header and no padding.

| Representation | Record | Bytes |
|----------------|--------|-------|
| D4 | `time_s` (float64), `polarity` (int8) | 9 |
| D5 | `arc_length_m` (float64), `radius_m` (float64), `polarity` (int8) | 17 |

`polarity` is +1 at a pit start and −1 at a pit end. Times are seconds from the origin above; arc length is metres along the spiral from the cutting position at time 0.

The descriptor has the same identity keys as a sampled tap, with `kind: edges`, plus:

| Descriptor key | Meaning |
|----------------|---------|
| `record_bytes` | 9 or 17 |
| `fields` | The record's fields, in order, with their types |
| `range_begin`, `range_end` | The span of the first field covered: records with `range_begin` ≤ value < `range_end` |
| `record_count` | Number of records in the file |
