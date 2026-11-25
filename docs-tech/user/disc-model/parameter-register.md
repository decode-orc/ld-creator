# Parameter register

Every value the disc model uses, with its source. A constant in the code
without a row here fails review (plan, "No unexplained formulas").

**Status:**

- `sourced`: taken from a cited document;
- `derived`: computed from sourced values, with the derivation in the stage note;
- `declared`: a modelling decision, with its reason and error bound in the stage note;
- `calibrated`: fitted to named measured data, with the method in the stage note;
- `unsourced-ideal`: no source yet, so the stage uses its ideal behaviour and the row is an open item;
- `open`: to be set by a named plan task.

**Project key:** the YAML key path, `fixed` for a value the model does not
expose, or `—` for a limit used only in validation or later phases.

IEC references are IEC 60857-1986 unless marked otherwise. "Amd" means an
amendment to it.

## Project-level keys

| ID | Name | Value | Tolerance | Unit | Source | Status | Project key |
|----|------|-------|-----------|------|--------|--------|-------------|
| P.name | Project name | — | — | — | videosynth convention | declared | `project.name` |
| P.version | Project version | — | — | — | videosynth convention | declared | `project.version` |
| P.description | Description | — | — | — | videosynth convention | declared | `project.description` |
| P.taps | Diagnostic taps | none | — | — | Plan, ground rules | declared | `diagnostics.taps` (and its entries' `stage`, `path`, `first_frame`, `frame_count`) |
| P.allow | Out-of-tolerance list | empty | — | — | Plan Task 1.6 decision | declared | `allow_out_of_tolerance` |
| P.mode | Output mode | `disc` | — | — | Plan, carrier diagnostic | declared | `output.mode` |
| P.rf_path | RF file | — | — | — | [S14](s14-rf-sample-writer.md) | declared | `output.rf_path` |

## S1 — CVBS input

| ID | Name | Value | Tolerance | Unit | Source | Status | Project key |
|----|------|-------|-----------|------|--------|--------|-------------|
| S1.video_path | Input recording | — | — | — | [S1](s01-cvbs-input.md) | declared | `input.video_path` |
| S1.first_frame | First frame | 0 | — | frame | [S1](s01-cvbs-input.md) | declared | `input.first_frame` |
| S1.frame_count | Frame count | all | — | frames | [S1](s01-cvbs-input.md) | declared | `input.frame_count` |
| S1.fh | Line frequency | 4.5 × 10⁶ / 286 = 15 734.265… | exact | Hz | SMPTE 170M-2004 | sourced | fixed |
| S1.fsc | Colour subcarrier | 455/2 · fH = 3 579 545.45… | exact | Hz | SMPTE 170M-2004 | sourced | fixed |
| S1.sample_rate | Input sample rate | 4 fsc = 14 318 181.81… | exact | Hz | CVBS spec `NTSC` preset | sourced | fixed |
| S1.frame_rate | Frame rate | fH / 525 = 30000/1001 | exact | Hz | SMPTE 170M-2004 | sourced | fixed |
| S1.line_samples | Samples per line | 910 | exact | — | CVBS spec `NTSC` preset | sourced | fixed |
| S1.blanking_code | Blanking level | 240 | `.meta` override | 10-bit | SMPTE 244M-2003 §4.2.1 | sourced | fixed |
| S1.white_code | White level | 800 | `.meta` override | 10-bit | SMPTE 244M-2003 §4.2.1 | sourced | fixed |
| S1.ire_scale | IRE scale | 100 IRE = white − blanking | exact | IRE | §9.1.6 note | sourced | fixed |
| S1.h_origin | 0H to stored sample 0 | 57/90 | exact | sample | CVBS spec, NTSC stored line alignment | sourced | fixed |

## S2 — Video rate converter

| ID | Name | Value | Tolerance | Unit | Source | Status | Project key |
|----|------|-------|-----------|------|--------|--------|-------------|
| S2.modelling_rate | Modelling rate `Fm` | 24 fsc = 6 × 4fsc = 945/11 MHz ≈ 85 909 091 | exact | Hz | [S2](s02-video-rate-converter.md): cubic Hermite phase error ≤ 1.45 ps against the 4 ps budget | derived | fixed |
| S2.kernel | Interpolation kernel | Kaiser-windowed sinc, cut-off 2fsc, ±20 input samples, β = 9; each phase scaled to unit DC gain | measured: ≤ 0.0005 dB, ≤ 0.08 ns to 6 MHz; images ≥ 91.5 dB down | — | [S2](s02-video-rate-converter.md) | declared | fixed |
| S2.passband_error | Allowed interpolation error | ≤ 0.01 dB, ≤ 0.1 ns, 0–6 MHz | — | — | [S2](s02-video-rate-converter.md) (a tenth of the S3/S4 tolerances) | derived | fixed |

## S3 — Group-delay pre-distortion

| ID | Name | Value | Tolerance | Unit | Source | Status | Project key |
|----|------|-------|-----------|------|--------|--------|-------------|
| S3.enabled | Pre-distortion applied | `true` | `false` is out of tolerance | — | §9.1.7 "shall" | sourced | `group_delay_predistorter.enabled` |
| S3.delay_points | Delay table, relative to 0.5 MHz | 0.5: 0; 2: −15; 3: −45; 3.58: −80; 4: −135; 4.2: −200 | ±15 at 2, 3, 3.58; ±30 at 4; ±50 at 4.2 | MHz: ns | §9.1.7, Figure 8 (Amd 2 replacement) | sourced | `group_delay_predistorter.delay_ns` |
| S3.magnitude | Magnitude response | flat | ±0.1 dB to 4.2 MHz | dB | §9.1.7 (delay only) | declared | fixed |
| S3.target_curve | Delay between table points | monotone cubic (Fritsch–Butland) through (0, 0) and the table, held beyond the last point | inside the Figure 8 band | ns | [S3](s03-group-delay-predistorter.md) | declared | fixed |
| S3.filter_taps | Filter length | 48 taps at 4fsc spacing (3.28 µs) | — | — | [S3](s03-group-delay-predistorter.md) | declared | fixed |
| S3.design_band | Least-squares fit | 0–6.8 MHz, 10 kHz grid; weight 1 to the last table point, 0.1 above; ridge 10⁻⁶ | — | — | [S3](s03-group-delay-predistorter.md) | declared | fixed |
| S3.bulk_delay | Removed constant delay | the filter span's centre, rounded to whole Fm periods (150 = 1 746.03 ns for the §9.1.7 table) | exact | — | [S3](s03-group-delay-predistorter.md) | derived | fixed |

## S4 — Video pre-emphasis

| ID | Name | Value | Tolerance | Unit | Source | Status | Project key |
|----|------|-------|-----------|------|--------|--------|-------------|
| S4.t1 | Zero time constant t₁ | 320 | ±8 | ns | §9.2.4, Figure 9 | sourced | `video_pre_emphasis.t1_ns` |
| S4.t2 | Pole time constant t₂ | 120 | ±2 | ns | §9.2.4, Figure 9 | sourced | `video_pre_emphasis.t2_ns` |
| S4.hf_gain | High-frequency gain | t₁/t₂ = 2.667 (+8.52 dB) | from t₁, t₂ | — | [S4](s04-video-pre-emphasis.md) | derived | fixed |
| S4.clipper | Overshoot clipping | none | — | — | Plan Task 1.6 decision | declared | fixed |
| S4.discretisation | Discrete filter | bilinear transform at Fm, no pre-warping, high-pass form with DC gain exactly 1 | measured ≤ 0.0055 dB, 0.1–6 MHz | — | [S4](s04-video-pre-emphasis.md) | declared | fixed |

## S5 — FM phase modulator

| ID | Name | Value | Tolerance | Unit | Source | Status | Project key |
|----|------|-------|-----------|------|--------|--------|-------------|
| S5.polarity | Modulation polarity | positive (white highest) | — | — | §9.2.1, §9.2.3 | sourced | fixed |
| S5.blanking_frequency | Carrier at blanking | 8 100 000 | ±50 000 | Hz | §9.2.2 | sourced | `fm_phase_modulator.blanking_hz` |
| S5.deviation | Sync tip → white deviation | 1 700 000 | ±35 000 | Hz | §9.2.3 | sourced | `fm_phase_modulator.deviation_hz` |
| S5.deviation_span | Span of the deviation | 140 (−40 to +100) | exact | IRE | §9.2.3 | sourced | fixed |
| S5.slope | Frequency per IRE | 1.7 MHz / 140 = 12 142.857 | from S5.deviation | Hz/IRE | [S5](s05-fm-phase-modulator.md) | derived | fixed |
| S5.initial_phase | Initial phase φ₀ | 0 | — | rad | [S5](s05-fm-phase-modulator.md) | declared | `fm_phase_modulator.initial_phase_rad` |
| S5.integration_rule | Phase integration over one Fm interval | (11, −93, 802, 802, −93, 11)/1440 on samples m−2 … m+3 | response error ≤ 2.2 × 10⁻⁵ to 6 MHz | — | [S5](s05-fm-phase-modulator.md) | derived | fixed |
| S5.phase_interpolant | φ between samples | cubic Hermite on φ and 2πf | ≤ 1.1 × 10⁻⁴ rad (S2 bound) | — | [S5](s05-fm-phase-modulator.md), [S2](s02-video-rate-converter.md) | declared | fixed |

## S6 — Recording signal combiner

| ID | Name | Value | Tolerance | Unit | Source | Status | Project key |
|----|------|-------|-----------|------|--------|--------|-------------|
| S6.slice_level | Slice level *s* (video only) | 0 | exact | — | [S6](s06-recording-signal-combiner.md) | derived | fixed |
| S6.audio_level | Audio subcarrier level (later) | −26 | ±1 | dB re carrier | §7.2.2 | sourced | — |
| S6.efm_level | EFM level (later) | −27 | ±1 | dB re carrier | Amd 2 §13.1.5 | sourced | — |

## S7 — Recording limiter

| ID | Name | Value | Tolerance | Unit | Source | Status | Project key |
|----|------|-------|-----------|------|--------|--------|-------------|
| S7.edge_budget | Edge-time error budget (S2 + S5 + S7) | 4 | — | ps | [S7](s07-recording-limiter.md): δf = 2f²δt ≤ 0.1 IRE at 12.15 MHz | derived | fixed |
| S7.pit_polarity | Pit half-cycle | cos φ > s | — | — | [S7](s07-recording-limiter.md) | declared | fixed |
| S7.root_solver | Edge solver | Newton in the interval bracket, bisection fallback; stops at a step of 2ε in u or 64 iterations | < 10⁻²³ s | — | [S7](s07-recording-limiter.md) | derived | fixed |
| S7.monotone_slope_ratio | Largest end slope of a monotone Hermite segment, relative to its secant | 3 | — | — | Fritsch & Carlson 1980; [S7](s07-recording-limiter.md) | sourced | fixed |

## S8 — Disc cutter

| ID | Name | Value | Tolerance | Unit | Source | Status | Project key |
|----|------|-------|-----------|------|--------|--------|-------------|
| S8.format | Disc format | required | `cav`, `clv` | — | §4.7.1, §4.7.2 | sourced | `disc_cutter.format` |
| S8.start_radius | Radius at the first frame | required | range 41.5–150; run must end ≤ 145 | mm | §4.2, §4.5.2, §4.10–4.12 | sourced | `disc_cutter.start_radius_mm` |
| S8.track_pitch | Track pitch | required (examples: 1.6667) | 1.4–2.0 | µm | §4.15; example value derived in [S8](s08-disc-cutter.md) | sourced | `disc_cutter.track_pitch_um` |
| S8.linear_velocity | CLV linear velocity | required for CLV | 10.1–11.4 | m/s | §4.7.2 | sourced | `disc_cutter.linear_velocity_m_s` |
| S8.cav_rotation | CAV rotation | 1 revolution per frame | exact | — | §4.7.1 | sourced | fixed |
| S8.clv_acceleration_limit | CLV angular acceleration | ≥ −0.32 | — | rad/s² | §4.7.3, Figure 19 | sourced | — |
| S8.rotation_sense | Rotation seen from read side | anticlockwise | — | — | §4.6 | sourced | fixed |
| S8.cutting_spot | Effective cutting spot | ≈ 0.6 (Phase 8) | — | µm | BBC RD 1986/01 §3.6 | sourced | — |
| S8.edge_profile | Cut edge profile | perfect step | — | — | [S8](s08-disc-cutter.md); mechanism: Isailović 1985 App. 2.2 (exposure contour, thresholds E₁, E₂) | unsourced-ideal | fixed |
| S8.length_bias | Pit length bias | 0 | order 0.1 µm in practice; acceptance bound ≈ ±0.13 µm at 55 mm, 8.1 MHz (40–60 % duty cycle) | m | [S8](s08-disc-cutter.md); Isailović 1985 §2.3.1.2, §2.3.1.6 | unsourced-ideal | fixed |

## S9 — Disc replicator

| ID | Name | Value | Tolerance | Unit | Source | Status | Project key |
|----|------|-------|-----------|------|--------|--------|-------------|
| S9.copy | Replication fidelity | exact copy | — | — | [S9](s09-disc-replicator.md); BBC §3.7; Isailović 1985 §2.5 (ideal is exact reproduction; departures unquantified) | unsourced-ideal | fixed |
| S9.substrate_index | Substrate refractive index | 1.525 (design point) | Figure 1a region | — | §5.1, Figure 1a | sourced | fixed |
| S9.substrate_thickness | Protective layer thickness | 1.225 (design point) | Figure 1a region | mm | §4.1.1, Figure 1a | sourced | fixed |
| S9.reflectivity | Reflectivity | ≥ 70 | — | % | §5.3 | sourced | — |
| S9.pit_geometry | Pit depth, width, wall slope | not represented | — | — | [S9](s09-disc-replicator.md) | unsourced-ideal | — |
| S9.pit_depth_optimum | Pit depth for maximum readout modulation | λ/(4n) = 103.7 | — | nm | Isailović 1985 §2.3.1.3, with S11.wavelength and S9.substrate_index | derived | — |
| S9.dropout_length_limit | Maximum dropout length | 100 | — | µs | §12.2.1 | sourced | — |

## S10 — Playback mechanics

| ID | Name | Value | Tolerance | Unit | Source | Status | Project key |
|----|------|-------|-----------|------|--------|--------|-------------|
| S10.lock | Playback time = recording time | exact | — | — | [S10](s10-playback-mechanics.md) | declared | fixed |
| S10.radial_deviation_limit | Radial deviation per revolution | ≤ 160 | — | µm | §4.18.1 | sourced | — |
| S10.radial_hf_limit | Radial deviation above 2.2 kHz | ±0.1 | — | µm | §4.18.2 | sourced | — |
| S10.time_base_error_limit | Time-base error | CAV: 10 µs p-p at 55 mm, 4 µs p-p at 145 mm (30 Hz); CLV: 10 µs p-p (30 Hz / 10 Hz) | — | µs p-p | §4.19.1 | sourced | — |
| S10.adjacent_track_shift_limit | Adjacent-track shift (CAV) | ±25 | — | ns | §4.19.2 | sourced | — |
| S10.vertical_deviation_limit | Vertical deviation | ±2 µm above 1.1 kHz; 100 m/s² below | — | — | §4.16 | sourced | — |
| S10.depth_of_focus | Information plane within depth of focus | CAV ±1.3 µm (110 mm dia) to ±2.5 µm (290 mm); CLV ±1.3 µm | — | µm | §11.3.2 | sourced | — |
| S10.tilt_limit | Optical axis tilt | ≤ 10 | — | mrad | §11.3.1 | sourced | — |

## S11 — Optical readout

| ID | Name | Value | Tolerance | Unit | Source | Status | Project key |
|----|------|-------|-----------|------|--------|--------|-------------|
| S11.wavelength | Wavelength λ | 632.8 | exact | nm | §11.1.1 | sourced | fixed |
| S11.numerical_aperture | Numerical aperture | 0.40 | ±0.01 | — | §11.1.2 (Amd 2 wording) | sourced | `optical_readout.numerical_aperture` |
| S11.rim_intensity | Rim-to-centre intensity | 0.23 | ±0.01 | — | §11.1.3 | sourced | `optical_readout.rim_intensity` |
| S11.beta | Gaussian pupil exponent β | −ln 0.23 = 1.4697 | from S11.rim_intensity | — | [S11](s11-optical-readout.md) | derived | fixed |
| S11.ring_limit | First-ring level | < 2 % (model: 0.72 %) | — | — | §11.1.4 | sourced | — |
| S11.cutoff | Spatial cut-off 2NA/λ | 1 264.2 | from NA, λ | cycles/mm | BBC §1.2; [S11](s11-optical-readout.md) | derived | fixed |
| S11.detection | Detector | whole returned beam on one photodiode | — | — | §11.2 | sourced | fixed |
| S11.model | Readout model | linear | — | — | Plan Task 5.2; phase-pit in Task 10.1 | declared | fixed |
| S11.hfmi_reference_frequency | HFMI spatial frequency | 802 | ±26 | pits/mm | Figure 18 (Amd 2) | sourced | fixed |
| S11.hfmi_limits | HFMI limits | 0.16–0.40 at 1.4 µm to 0.18–0.42 at 2.0 µm (read from the figure, ±0.01), linear in pitch between; no band outside 1.4–2.0 µm | — | — | Figure 18 (Amd 2) | sourced | — |
| S11.mtf_at_reference | H at 802 pits/mm | 0.21317 | < 10⁻¹² (quadrature, checked against the uniform-pupil closed form) | — | [S11](s11-optical-readout.md) | derived | fixed |
| S11.hfmi | HFMI of the modelled disc | 0.27142 (m = 1) | inside Figure 18 | — | [S11](s11-optical-readout.md) | derived | `optical_readout.hfmi` |
| S11.readout_rate | Readout rate Fr | 80 | exact | MHz | [S11](s11-optical-readout.md): band limit 35.7 MHz < 40 MHz | derived | fixed |
| S11.adjacent_tracks | Adjacent-track content | blank | — | — | [S11](s11-optical-readout.md) | declared | fixed |
| S11.kernel_window | Readout kernel window | LSF unchanged to ±10 µm, raised cosine to 0 at ±20 µm; far field from the pattern's mean | transfer within 1.1 × 10⁻⁴ of H from 110 cycles/mm | — | [S11](s11-optical-readout.md), Rendering | declared | fixed |
| S11.kernel_table_spacing | Edge-response table spacing | 5 | interpolation < 10⁻¹⁰ per edge | nm | [S11](s11-optical-readout.md), Rendering | declared | fixed |
| S11.line_spread_quadrature | LSF quadrature | ν = fc(1 − t²), 32 panels × 16-point Gauss–Legendre | — | — | [S11](s11-optical-readout.md), Rendering | declared | fixed |
| S11.record_boundary | Pattern outside the record | its mean (d = 0) | affects 20 µm at each end | — | [S11](s11-optical-readout.md), Rendering | declared | fixed |
| S11.finite_pit_width_error | Error of the stripe approximation (0.8 µm pit) | shape +8.7 % to −18.9 % over 300 cycles/mm–0.833·fc; ±5 % over the carrier band at 55 mm; long-pit depth 0.72 | — | — | [S11](s11-optical-readout.md), Assumptions | derived | — |

## S12 — Player front end

| ID | Name | Value | Tolerance | Unit | Source | Status | Project key |
|----|------|-------|-----------|------|--------|--------|-------------|
| S12.profile | Preamplifier response | `iec_reference` (flat, unity, noiseless) | — | — | §11.2 | sourced | `player_front_end.profile` |
| S12.mtf_equaliser | MTF equaliser | `none` (test-point RF); diagnostic `inverse`: G = 1/H(f/v) below fc·v, 0 above | — | — | [S12](s12-player-front-end.md); ld-decode `-m/--MTF` (decoder compensates); Isailović 1985 §3.4 | declared | `player_front_end.mtf_equaliser` |
| S12.equaliser_kernel | `inverse` kernel | Kaiser-windowed sinc, A = 122 dB (β = 0.1102(A − 8.7)), W = 20 µm, fe = fc − Δν/2, Δν = (A − 7.95)/(14.36 · 2W) | 1 ± 10⁻⁶ to 0.843·fc; < 10⁻⁶ from fc | — | Kaiser 1974; [S12](s12-player-front-end.md) | declared | fixed |

## S13 — RF capture

DdD = DomesdayDuplicator repository, commit `0f015fa0c5d7`.

| ID | Name | Value | Tolerance | Unit | Source | Status | Project key |
|----|------|-------|-----------|------|--------|--------|-------------|
| S13.profile | Capture device | `domesday_duplicator` | — | — | Plan Task 1.6 decision | declared | `rf_capture.profile` |
| S13.level_fraction | Reference level | 0.75 of full scale: the run's RF peak-to-peak | — | — | DdD hardware guide gain table ("Aim for (75%)") and "Measuring RF signal amplitude"; hardware testing guide | sourced | `rf_capture.level_fraction` |
| S13.sample_rate | Sample rate | 40 | exact | MHz | DdD hardware guide, ADC and FPGA | sourced | fixed |
| S13.adc_bits | ADC resolution | 10 (DdD); 16 (`ideal`) | — | bit | DdD schematic (ADS825); [S13](s13-rf-capture.md) | sourced | fixed |
| S13.adc_full_scale | ADC input range | 2.0 p-p, centred on 2.5 V | — | V | DdD hardware guide, ADC | sourced | fixed |
| S13.input_termination | Input load | 56 Ω ∥ 500 Ω = 50.36 | — | Ω | DdD schematic note | sourced | fixed |
| S13.ac_coupling | AC-coupling corner | 1/(2π · 500 Ω · 100 nF) = 3.18 | — | kHz | DdD schematic (C401, R402, R403) | derived | fixed |
| S13.filter_components | Anti-alias filter, nominal (comparison only; the model uses S13.filter_fit) | L401 680 nH, L402 560 nH, C403 330 pF, C404 33 pF, C405 470 pF, C406 100 pF, C407 270 pF, R410 = R411 = 47 Ω | capacitors 5 % | — | DdD schematic | sourced | fixed |
| S13.filter_fit | Anti-alias filter, effective values | C403 268.1 pF, L401 622.7 nH, C404 58.48 pF, C405 444.0 pF, L402 566.4 nH, C406 64.30 pF, C407 396.7 pF (R410, R411 nominal) | reproduces sheet 1 within 0.17 dB to 17.8 MHz and sheet 2 within 6° to 16 MHz | — | Weighted least squares to DdD spreadsheet sheets 1 and 2 ([S13](s13-rf-capture.md), `scripts/fit_ddd_filter.py`) | calibrated | fixed |
| S13.amplifier_bandwidth | OPA690 closed-loop bandwidth | single pole at 250 MHz / 8.5 = 29.4 | typical (no minimum given) | MHz | TI SBOS223H table 6.8 (OPA690ID, VS = 5 V, GBP for G ≥ 10); noise gain 1 + R406/R405 | sourced | fixed |
| S13.filter_topology | Filter connections | R410 → A; C403 A–gnd; L401 ∥ C404 A–B; C405 B–gnd; L402 ∥ C406 B–C; C407 C–gnd; R411 C → ADC (unloaded) | — | — | DdD board netlist (`Domesday Duplicator.kicad_pcb`); single termination confirmed by sheet 1 at 1 MHz | sourced | fixed |
| S13.amplifier_feedback | Amplifier feedback | R405 200 Ω, C402 100 nF, A = R406 1.5 kΩ (switch "1000"): shelf zero 936 Hz, pole 7.96 kHz, normalised to unity at high frequency | — | — | DdD schematic; sheet 1 conditions | sourced | fixed |
| S13.filter_realisation | Realisation at Fr | first-order sections by the bilinear transform; LC filter as an FIR from 2^15 response samples with a smooth step 36–40 MHz | sections within 1.2 × 10⁻⁴; FIR within 10⁻⁸ to 35.7 MHz | — | [S13](s13-rf-capture.md) | declared | fixed |
| S13.filter_truncation | FIR truncation | Σ\|h\| outside the kept taps ≤ 10⁻⁹ of the total (757 taps) | — | — | [S13](s13-rf-capture.md) | declared | fixed |
| S13.capture_mean | D9 mean and padding | 1 − m/2, subtracted before the filters; also D9 before τ = 0 and after the run | — | — | [S13](s13-rf-capture.md) | declared | fixed |
| S13.level_reference | Level computation | two passes: the first measures the extremes of the filtered D9 at the converter's samples, outside the FIR's padding; the second scales their peak-to-peak to 2·level_fraction | exact at the samples | — | [S13](s13-rf-capture.md) | declared | fixed |
| S13.filter_measured | Measured front-end response | sheet 1 (magnitude) and sheet 2 (phase) tables | — | dB | DdD gain and filter spreadsheet | sourced | — |
| S13.quantiser | Quantiser | round(512 + 512u), clamped to 0–1023 | ±0.5 LSB | code | [S13](s13-rf-capture.md) | declared | fixed |
| S13.ideal_quantiser | `ideal` profile quantiser | round(32768u), clamped to −32768…32767 | ±0.5 LSB | code | [S13](s13-rf-capture.md) | declared | fixed |
| S13.ideal_filter | `ideal` profile anti-alias filter | passband 0–15 MHz, ripple < 0.01 dB; ≥ 98 dB down from 20 MHz; realised as a 105-tap zero-phase Kaiser sinc, cut-off 17.5 MHz, designed for A = 100 dB | — | — | [S13](s13-rf-capture.md); Kaiser 1974 | declared | fixed |
| S13.sample_origin | First sample instant | τ = 0 | — | s | [S13](s13-rf-capture.md) | declared | fixed |

## S14 — RF sample writer

| ID | Name | Value | Tolerance | Unit | Source | Status | Project key |
|----|------|-------|-----------|------|--------|--------|-------------|
| S14.ddd_word | 16-bit word of a 10-bit code | (c − 512) × 64 | exact | — | DdD `capture-files.md`; `test_sample_format.cpp` | sourced | fixed |
| S14.format | File format | `.s16`, signed 16-bit little-endian mono, no header | — | — | ld-decode input interface | sourced | fixed |

## Unsourced and open rows

| Row | What would resolve it |
|-----|-----------------------|
| S8.edge_profile, S8.length_bias | Model form and a bound now sourced (Isailović 1985 App. 2.2, §2.3.1.6); values still need resist and spot data: Jacobs (Applied Optics 1978); Pasman et al. (SPIE 529, 1985); Bouwhuis et al. (1985), Mastering chapter |
| S9.copy, S9.pit_geometry | Mechanisms sourced (Isailović 1985 §2.3, §6.2: edge blur by metallization, bump shape); values need Bouwhuis et al. (1985), Legierse & Pasman (1989), or measurement of a pressed disc |
| S11.model (linear) | Plan Task 10.1, with Hopkins (JOSA 1979) or Bouwhuis et al. (1985), "Read-out of optical discs" |
| S13 quantiser transition points | The ADS825 datasheet (TI SBAS069B) gives no per-code transitions, only DNL and INL bounds, so the declared `Q` stands; a measured ADC (code histogram) would resolve it |
| S13.filter_fit (one board) | Fitted to one board's measurement; the physical cause of the nominal model's 0.5–2.9 dB gap is unknown. A network-analyser measurement of a second board would show whether the fit is typical |

The candidate sources are described in
[secondary-sources.md](secondary-sources.md).
