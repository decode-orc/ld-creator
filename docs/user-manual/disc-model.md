# Disc Model

A disc run (`output.mode: disc`, the default) turns the video recording into the RF a capture device would record from a LaserDisc mastered with it. Each physical step between the two is a stage of its own, with its settings in a block of the [project file](../reference/project-file.md).

!!! warning "Partly implemented"
    The disc carries video only, and every stage runs at its ideal setting: there is no noise, no crosstalk from neighbouring tracks, and no eccentricity or other disc defect. Audio, EFM and PAL are planned.

## What a disc run does

| Stage | Block | What it models |
|-------|-------|----------------|
| Video input to FM modulator | `input` … `fm_phase_modulator` | Rate conversion, group-delay pre-distortion (IEC 60857 §9.1.7), pre-emphasis (§9.2.4) and frequency modulation (§9.2.1–9.2.3), as in a [carrier diagnostic](carrier-diagnostic.md) |
| Recording limiter | — | The recorder cuts in two levels only: a pit edge wherever the carrier crosses zero |
| Disc cutter | `disc_cutter` | Places each edge on the spiral track, at the CAV or CLV speed of §4.7, from `start_radius_mm` |
| Replication | — | The pressed disc is an exact copy of the master |
| Playback mechanics | — | The player is locked to the disc and tracks perfectly |
| Optical readout | `optical_readout` | The reading spot of the IEC reference optics (§11.1: 632.8 nm, NA 0.40, 23 % rim intensity) blurs the pits. The detector signal is 1 on unpitted land |
| Player front end | `player_front_end` | A flat preamplifier: the RF as at a player's RF test point |
| Capture | `rf_capture` | The capture device's filters, level and converter, sampled at 40 MHz |

The [RF output](../reference/rf-output.md) has the same length as a carrier-diagnostic run of the same input: every 40 MHz sample before the end of the last frame.

## The readout loss

The reading spot cannot resolve detail finer than its optical cut-off, 2·NA/λ = 1264 cycles per mm. At the inner CAV radius of 55 mm the disc passes the spot at 10.4 m/s, so the cut-off is 13.1 MHz; at 145 mm it is 34.5 MHz. The higher parts of the FM spectrum therefore come out weaker at the inner radius, and chroma demodulates larger relative to luma there. This is real physics, so it stays in the RF: ld-decode compensates it itself.

`optical_readout.hfmi` sets the depth of modulation. The default, 0.271, is the most this readout model can give; it lies inside the IEC 60857 Figure 18 band.

## Capture profiles

| `rf_capture.profile` | Filters | Converter |
|----------------------|---------|-----------|
| `domesday_duplicator` (default) | The Domesday Duplicator's AC coupling, amplifier and elliptic anti-alias filter, computed from its circuit | 10-bit, stored as an uncompressed Domesday Duplicator capture |
| `ideal` | A linear-phase low-pass: flat within 0.01 dB to 15 MHz, at least 98 dB down from 20 MHz | 16-bit |

The level is set so that the blanking-level carrier at the start radius fills `level_fraction` (75 %) of the converter's range. Lower carrier frequencies, such as sync, are attenuated less by the readout and use the headroom above it: about 85 % of full scale. If any sample still reaches the end of the range, the run warns how many were clipped.

## Decoding

| Project | ld-decode |
|---------|-----------|
| Default (`mtf_equaliser: none`) | Default options: ld-decode compensates the readout loss |
| `player_front_end.mtf_equaliser: inverse` | `-m 0`: the RF is already compensated |
| `output.mode: carrier_diagnostic` | `-m 0`: there is no readout |

The `inverse` equaliser is a diagnostic. It removes the readout's loss up to 84 % of the optical cut-off, so comparing it with a default run shows whether a fault lies in the readout model or in the decoder's compensation of it.

## Run time

The readout and the capture filter dominate. A run takes about a second per frame with eight threads, and the RF is byte-identical for any `--threads`.

## Diagnostic taps

Every stage's output can be written to a file; see [Diagnostic taps](../reference/diagnostic-taps.md).
