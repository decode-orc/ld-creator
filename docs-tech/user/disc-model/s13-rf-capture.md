# S13 — RF capture (`rf_capture`)

## Physical process

A capture device digitises the player's RF output. The reference device is
the **DomesdayDuplicator** (DdD): a 50 Ω input, an AC-coupled
non-inverting amplifier with switch-selected gain, a passive elliptic
anti-alias filter, and a 10-bit ADC at 40 MHz. Its samples are what ld-decode
is normally given.

## Sources

All from the DomesdayDuplicator repository
(github.com/simoninns/DomesdayDuplicator), commit `0f015fa0c5d7`, used with
the owner's permission:

- `hardware/pcb/RF Amplifier.kicad_sch` (rev 3.0.2): component values.
- `hardware/pcb/ADC.kicad_sch`: ADS825 ADC and its reference network.
- `hardware/doc/DdD Gain and filter calculations.xlsx`: sheet 1, measured
  amplitude response of the whole front end, 10 kHz–21.5 MHz at 1/12-decade
  steps (200 mV p-p input, gain switch "1000"); sheet 2, measured phase
  of the same board, 0–16 MHz in 250 kHz steps; sheet 3, the gain table.
  Sheets 1 and 2 are measurements of a real board, not design predictions.
- `docs/content/development/hardware-guide.md`: input stage, gain theory,
  filter stage, ADC and FPGA clocking.
- `docs/content/development/hardware-testing.md`: the 75 % working level,
  and the capture application's amplitude readout (√2 × RMS of the
  captured signal, as a fraction of full scale).
- TI OPA690 datasheet (SBOS223H, 2024), table 6.8 (OPA690ID, VS = 5 V):
  gain-bandwidth product 250 MHz typical for G ≥ 10. The DdD fits the
  SOIC-8 (D) package (board footprint) on a single +5 V supply
  (schematic).
- `docs/content/capture-gui/capture-files.md`: sample format of captures.
- `ddd-gui/tests/unit/test_sample_format.cpp`: the 16-bit representation of
  a 10-bit code.

## Model and equations

The chain, in signal order:

| Block | Components | Model | Source |
|-------|------------|-------|--------|
| Input termination | R401 56 Ω ∥ bias divider (R402, R403 1 kΩ to the ADC references → 500 Ω) | 50.36 Ω load | Hardware guide, "Input impedance" |
| AC coupling | C401 100 nF into the 500 Ω divider | First-order high-pass, `1/(2π · 500 Ω · 100 nF)` = 3.18 kHz | Schematic |
| Amplifier | OPA690, non-inverting, gain `1 + A/(B + 1/(s·C402))`, B = R405 = 200 Ω, C402 = 100 nF, A = the switched parallel combination of R406–R409 (1.5 k, 1 k, 680, 560 Ω) | Gain 2.02–8.5 in 15 settings above a few kHz (unity at DC). Modelled at switch "1000" (A = 1.5 kΩ, the measured setting): a shelf with its zero at 936 Hz and pole at 7.96 kHz, normalised to unity at high frequency; and the closed-loop bandwidth as a single pole at GBP / noise gain = 250 MHz / 8.5 = 29.4 MHz | Hardware guide, "Operational amplifier gain theory"; sheet 3; schematic; OPA690 datasheet table 6.8 |
| Anti-alias filter | L401 680 nH, L402 560 nH, C403 330 pF, C404 33 pF, C405 470 pF, C406 100 pF, C407 270 pF (5 %), R410 and R411 47 Ω | Passive LC elliptic low-pass driven through R410; transfer function from the netlist with effective component values fitted to the measured board (below) | Schematic; board netlist; hardware guide, "Filter stage"; sheets 1 and 2 |
| ADC | ADS825, 10-bit, 2 V p-p full scale centred on 2.5 V | Sampling at 40 MHz, quantisation to codes 0–1023 | Hardware guide, "ADC"; schematic |
| Clock | 80 MHz FPGA PLL from the DE0-Nano 50 MHz oscillator, divided by 2 | Exact 40 MHz at the ideal setting | Hardware guide, "DE0-Nano FPGA" |

**Filter model (decision, revised).** Plan Task 5.3 computed the
filter's transfer function from the netlist with nominal component values,
then checked it against the measured response (sheet 1), which includes
the input stage and amplifier. It does not match from 6.8 to 12.1 MHz
("Comparison with the measurement", below), so the model now uses
**effective component values fitted to the measured board** (register
`S13.filter_fit`), in the netlist's topology, together with the amplifier's
datasheet bandwidth. Fitting to measured data is allowed because the
dataset is named (sheets 1 and 2) and the method documented
(`scripts/fit_ddd_filter.py`). The fitted values describe this board's
response, not its parts; the nominal values are kept for comparison. The
guide reports −3 dB at about 13.8 MHz and less than −35 dB at 20 MHz;
sheet 1, taken relative to its 1 MHz value, gives −3.3 dB at 12.1 MHz and
−28.4 dB at 17.8 MHz. The filter's group delay is modelled, not only its
magnitude: the DdD documentation itself notes that delay that varies with
frequency disperses an FM signal (`fpga-decimation-filter.md`, "Why this
filter").

**Netlist.** The connections are read from the board's netlist
(`Domesday Duplicator.kicad_pcb`, pad nets, same commit), which is what was
fabricated; the schematic's drawing has crossing wires without junction
dots that make an automatic extraction ambiguous. In signal order: the
OPA690 output drives R410 into node A; C403 shunts A; L401 ∥ C404 joins A
to B; C405 shunts B; L402 ∥ C406 joins B to C; C407 shunts C; R411 takes C
to the ADS825 input (`/ADC/IN`), whose only other connection is the ADC.
The ADC input is taken as unloaded, so the output is the voltage at C and
R411 carries no current. Sheet 1 confirms the single termination: at
1 MHz it measures −1.47 dB relative to the ADC's maximum, where
200 mV p-p × 8.5 into the 2 V p-p window, with no termination loss, is
−1.41 dB (a doubly terminated filter would be 6 dB lower). The two traps
resonate at 33.6 MHz (L401 ∥ C404) and 21.3 MHz (L402 ∥ C406).

**Comparison with the measurement: nominal components** (sheet 1, at
the measured gain; unit tests). Up to 5.6 MHz, and at 14.7 and 17.8 MHz,
the measured response lies within the span of the nominal model over every
±5 % corner of the five capacitors, widened by 0.2 dB for the measurement
and the gain resistors (at low frequency the measurement is 0.12 dB above
the model, consistent with the resistor tolerance). **From 6.8 to 12.1 MHz
it does not**: the measurement is 0.5 to 2.9 dB below the nominal model and
below the whole capacitor span, with the largest gap at 12.1 MHz. The
measured phase (sheet 2) fits the nominal chain plus a pure delay of 11 ns
up to 10 MHz (1.7° rms); from 6 to 10 MHz it lags by a further 3° to 8°,
and from 13 MHz it rises faster than the model, by 13° at 13.5 MHz and 36°
at 14.5 MHz. Both point to a real filter edge somewhat lower than the
nominal one.

Physically plausible changes do not close the gap. With each capacitor
within ±5 %, each inductor within ±10 % and the amplifier's closed-loop
pole free (the fit drove it to 15.7 MHz, half the datasheet's 29.4 MHz),
0.7–0.9 dB remains from 10 to 14.7 MHz. Inductor loss (Q from 20 to 60)
and a load capacitance up to 10 pF do not close it either. The cause is not
known; the remaining candidates are component values beyond those
tolerances, the OPA690's output impedance into the filter's first
capacitor, and the measurement setup.

**The fit** (register `S13.filter_fit`; `scripts/fit_ddd_filter.py`). The
seven reactive components are fitted, in the netlist's topology, by
weighted least squares to sheet 1's magnitude up to 17.8 MHz (0.1 dB per
unit) and sheet 2's phase up to 15 MHz (2° per unit), with the first-order
sections and the 29.4 MHz amplifier pole fixed. Two nuisance terms are
fitted and not modelled: a gain offset (+0.11 dB, the gain resistors,
absorbed by the level setting) and a pure delay (3.7 ns, the measurement
path). Twelve random starts within ±15 % of nominal, bounds 0.4–2.5 ×
nominal; 28 further starts all reach the same minimum, with responses
within 0.001 dB.

| Component | Nominal | Effective | Ratio |
|-----------|---------|-----------|-------|
| C403 | 330 pF | 268.1 pF | 0.812 |
| L401 | 680 nH | 622.7 nH | 0.916 |
| C404 | 33 pF | 58.48 pF | 1.772 |
| C405 | 470 pF | 444.0 pF | 0.945 |
| L402 | 560 nH | 566.4 nH | 1.011 |
| C406 | 100 pF | 64.30 pF | 0.643 |
| C407 | 270 pF | 396.7 pF | 1.469 |

The fitted chain reproduces sheet 1 within 0.053 dB rms and 0.17 dB at
worst to 17.8 MHz (the gap was 2.9 dB), and sheet 2 within 1.6° rms to
13 MHz and 1.9° rms (6.0° at worst) to 16 MHz. The two traps move to
26.4 MHz. At 21.5 MHz the measured −43.5 dB is an instrument's floor, not
the filter, and is not fitted.

Relative to 1 MHz, the fitted response is −1.1 to −3.0 dB from 8 to 13 MHz,
where the nominal model was −0.3 to +1.2 dB. That band carries the FM
signal's upper chroma sidebands (11.7–12.9 MHz), so the fit changes the
captured chroma sidebands' balance. A reference decoder that does not
compensate the capture filter reads chroma 15.2 % low at 145 mm through
the fitted filter, and 18.6 % low through the nominal one (round-trip
metrics, "Results").

**Above 20 MHz** the fitted chain is at least 41.1 dB below its 1 MHz
level, at 20 MHz itself (the nominal chain: 47.3 dB; the guide's design
tool predicted 42 dB at 20 MHz; the board measured less than 35 dB, at its
instrument's floor).

**Realisation at Fr** (register `S13.filter_realisation`). The two
first-order sections (AC coupling, amplifier shelf) are bilinear
transforms at Fr. Their corners are below 10 kHz, so frequency warping
only drops what is left of their phase tail near Nyquist: at most 1.2e-4
(−78 dB) at any frequency. The LC filter becomes an FIR: its response is
sampled at 2^15 frequencies across Fr, multiplied by a smooth step from 1
at 36 MHz to 0 at 40 MHz (D9 holds nothing above 35.7 MHz, so this changes
nothing that reaches the filter, but makes the periodic response smooth),
and inverse-transformed. Taps are kept over the shortest span outside which
Σ|h| is below 1e-9 of the total (register `S13.filter_truncation`): 757
taps, 265 of them before tap 0. The precursor is the band limit's, not the
circuit's: a causal filter's response limited to 40 MHz is not causal. A
raised-cosine step in place of the smooth one leaves tails falling only as
n⁻³ and needs 4121 taps. The FIR matches the circuit within 1e-8 to
35.7 MHz (unit test).

**Mean and start-up** (register `S13.capture_mean`, declared). D9's mean
for a 50 %-duty pit train is 1 − m/2 (S11). It is subtracted before the
filters, and D9 is taken to equal it before τ = 0 and after the end of the
run, so the filters start in the steady state of a disc that was already
playing; with the `ideal` profile, which has no AC coupling, the
subtraction centres the signal on the converter as the DdD's coupling
does. Only the first and last ~3 µs (the FIR's span) see the padding.

**Sampling.** D9 is at Fr = 80 MHz (S11). After the continuous-time chain
(realised at Fr), the ADC samples every second value:

```
D10[n] = Q( level · y(τ = n / 40 MHz) ),   sample 0 at τ = 0
```

Taking every second sample is exact because D9 is band-limited below 40 MHz
(S11), and the anti-alias filter has already removed the content above
20 MHz that would otherwise alias into the capture, as the real hardware
does.

**Level (decision, revised 2026-11-25).** The DdD guide sets the working
level from the player's RF peak-to-peak: measure it on an oscilloscope
(hardware guide, "Measuring RF signal amplitude"), then choose the gain so
that it fills 75 % of the converter's 2 V p-p window ("Aim for (75%)"; the
rest is headroom, "so that signal peaks do not clip", hardware testing
guide). The model does the same in two passes over the run. The first runs
the whole chain at unit scale and records the extremes of the filtered D9
at the converter's samples; the second scales so that their peak-to-peak
is `level_fraction` × full scale:

```
scale = 2 · level_fraction / (y_max − y_min)
```

The extremes are taken only where the FIR reads no padding (the first and
last ~3 µs of the run), because the padding's start-up is not signal. They
are taken at the converter's samples, so the run cannot clip unless its
waveform is more than 2:1 asymmetric about its centre. On programme
material, whose carrier is not locked to the sampling, the samples reach
the waveform's peaks; a carrier locked to it (an exact 8 MHz) would be
sampled at fixed phases and read up to 3 % below its analogue
peak-to-peak. The level therefore depends on the run's content, as two
discs set up on a scope would. The resolved project records the measured
peak-to-peak and scale (`resolved.capture_peak_to_peak`,
`resolved.capture_scale`), for information; a rerun measures them again.
The second pass doubles a disc run's time. Clipped samples are still
counted and reported, as the DdD capture application does.

The previous rule set the level from the blanking carrier alone, computed
analytically, and lower carriers, which the readout attenuates less, then
rose above 75 % and, with the fitted board response, clipped. That rule
did not follow the guide.

**Quantisation (declared).** `Q(u) = clamp(round(512 + 512·u), 0, 1023)`,
with `u` the ADC input relative to half the full scale (±1 V → ±1). Error:
±0.5 LSB. The ADS825's exact code transition points are an open item.

### `ideal` profile

For bisecting faults, the `ideal` profile replaces the DdD chain with a
linear-phase anti-alias low-pass at Fr and a 16-bit quantiser:

- passband 0–15 MHz with ripple below 0.01 dB, which covers the 12.2 MHz
  maximum carrier excursion (S4);
- stopband from 20 MHz at least 98 dB down, below 16-bit quantisation, so
  aliasing is below the quantisation floor;
- the same level reference;
- quantisation (declared) `Q(u) = clamp(round(32768·u), −32768, 32767)`,
  the 16-bit counterpart of the DdD rule, ±0.5 LSB.

The filter (register `S13.ideal_filter`) is a Kaiser-windowed sinc at Fr,
cut-off 17.5 MHz, designed for A = 100 dB over the 5 MHz transition
(β = 0.1102·(A − 8.7), length from D = (A − 7.95)/14.36): 105 taps,
symmetric about tap 0, so zero phase. Unit tests check it against the
specification above: within 0.01 dB to 15 MHz with zero phase, more than
98 dB down from 20 to 40 MHz. (Designing for A = 98 dB itself leaves too
little margin at the band edges.)

`round` is to the nearest integer, halves away from zero, in both profiles.
Implemented in `src/rf_capture/`: the quantisers
(`capture_quantiser.cpp`), the filters (`capture_filter.cpp`) and the stage
with its level reference (`rf_capture.cpp`).

### Carrier diagnostic

`output.mode: carrier_diagnostic` (plan Task 3.5) bypasses S6–S12 and this
stage's filter, but uses its sampling grid and quantiser. Capture sample n,
at τ = n / 40 MHz, falls at n·189/88 Fm samples, and u = level_fraction ·
cos φ(τ). The carrier has a constant amplitude, so its peak-to-peak is
exactly level_fraction of full scale, which is the level rule above applied
to it, with no measuring pass. It is not a disc.

## Parameters

| Register row | Value |
|--------------|-------|
| `S13.profile` | `domesday_duplicator` |
| `S13.sample_rate` | 40 MHz (fixed) |
| `S13.adc_bits` | 10 (DdD), 16 (`ideal`) |
| `S13.quantiser`, `S13.ideal_quantiser` | The two Q rules above |
| `S13.adc_full_scale` | 2 V p-p |
| `S13.level_fraction` | 0.75 |
| `S13.filter_components` | schematic values above (comparison only) |
| `S13.filter_fit` | effective component values above, fitted to sheets 1 and 2 |
| `S13.amplifier_bandwidth` | single pole at 250 MHz / 8.5 = 29.4 MHz |
| `S13.ac_coupling_corner` | 3.18 kHz |
| `S13.amplifier_feedback` | R405 200 Ω, C402 100 nF, A = R406 1.5 kΩ (switch "1000") |
| `S13.filter_topology` | board netlist, single termination (above) |
| `S13.filter_realisation` | bilinear sections; FIR from 2^15 response samples, smooth step 36–40 MHz |
| `S13.filter_truncation` | Σ\|h\| outside the kept taps ≤ 1e-9 of the total |
| `S13.capture_mean` | 1 − m/2, subtracted; also D9 outside the run |

## Assumptions and approximations

- **Noiseless.** The ADC clock interference the guide describes, ADC noise
  and amplifier noise are Phase 9 work.
- **Amplifier bandwidth as one pole** at the datasheet's typical
  gain-bandwidth product over the noise gain. The datasheet gives no
  minimum. Because the level is set by
  `level_fraction`, the gain-switch setting only moves the shelf, below
  10 kHz, where the FM signal has no content, so it is not a key in this
  scope; the model uses the measured setting.
- **Fitted components.** The filter's response is fitted to one board's
  measurement. Another board will differ by its component tolerances; the
  nominal model's ±5 % span shows how much (above).
- **Exact clock.** The DE0-Nano oscillator tolerance is a later impairment.

## Input/output contract

- **In:** D9 at Fr. **Out:** D10 — integer codes at 40 MHz (0–1023 for DdD;
  signed 16-bit for `ideal`), sample 0 at τ = 0.

## Project YAML block

`rf_capture`:

| Key | Type | Default | Range |
|-----|------|---------|-------|
| `profile` | string | `domesday_duplicator` | `domesday_duplicator`, `ideal` |
| `level_fraction` | float | 0.75 | 0.05–1.0 |

## Ideal setting

The DdD profile, as decided in plan Task 1.6: the reference capture is the
device real captures are made with. The `ideal` profile is an alternative for
A/B comparisons.

## Verification

`tests/unit/test_rf_capture.cpp`: the level arithmetic; a carrier's
peak-to-peak at 75 % of full scale through the two passes, for both
profiles, with and without the equaliser; the nominal response against sheet 1
(above, with the deviation that led to the fit guarded at its recorded
size); the fitted chain against sheets 1 and 2 (0.18 dB, 6°); the FIR and
sections against the continuous responses; attenuation above 20 MHz for
both profiles; the ideal filter's passband; no clipping for
pit trains at 7.6, 8.1, 9.3 and 12.15 MHz at 55 mm and 145 mm CAV and on
CLV, with and without the equaliser; block-size independence and exact
decimation. `tests/functional/test_disc_output.cpp`: no clipped sample in a
disc run of SMPTE bars at 55 mm.

## Diagnostic tap

`rf_capture`: the converter input `u = level·y` at Fr before sampling, as
`float64` (±1 = the converter's range; representation `D9 filtered`). When
it is declared the FIR is evaluated at every Fr sample; otherwise only at
the samples the converter takes.

## Open items

- The physical cause of the nominal model's 0.5–2.9 dB gap from 6.8 to
  12.1 MHz, now covered by the fit (above). A network-analyser measurement
  of a second board would show whether the fit describes boards in
  general or this one.
- Headroom of the level rule: resolved by the measured level above. The
  blanking-carrier rule it replaces clipped on the round-trip material.
- ADS825 transfer function. The datasheet (TI SBAS069B, 2002) is now read;
  it has no per-code transition data, so the declared `Q` stands. The DdD
  ties RSEL high and INT/EXT low (ADC schematic), which the datasheet
  (p. 9) names as the 2 V p-p, internal-reference configuration. Figures for
  a later non-ideal ADC: straight offset binary, REFB → code 0, REFT →
  full scale (Table I, p. 12), matching `Q`; DNL ±0.25 typ / ±1.0 max LSB
  and INL ±0.5 typ / ±2.0 max LSB at 1 MHz; no missing codes "Tested";
  gain error including the internal reference ±1.5 typ / ±3.5 max % FS;
  zero error ±1.0 typ / ±3.0 max % FS; REFT and REFB ±25 mV max; SNR
  60 dB and SINAD 58 dB typ at 10 MHz; output noise 0.2 LSB rms; aperture
  delay 3 ns, jitter 1.2 ps rms; input bandwidth 300 MHz; latency 5 clocks
  (pp. 2–4). Gain and zero error are specified at 25 °C and 2.5 MHz only.
- DE0-Nano oscillator tolerance (later clock-error impairment). Terasic's
  schematic marks Y1 only "50MHZ", with no part or ppm. The DdD clock is
  exactly 50 MHz × 8/5 ÷ 2 (`fpga/common/IPpllGenerator.v`; ADC clock
  toggled from the 80 MHz PLL output in `DomesdayDuplicator.v`), so the
  oscillator's error reaches the sample rate unchanged. Approximate value:
  ±50 ppm, typical of 3.3 V 50 MHz SMD oscillators (ECS-3225MV, SiT8008:
  ±20 to ±100 ppm). Cyclone IV PLL output jitter on a regular I/O pin is at
  most 75 mUI peak-to-peak (Table 1–25), about 67 ps rms at 80 MHz as a
  worst-case bound, against the ADC's own 1.2 ps
  ([approximate figures](secondary-sources.md#approximate-figures-from-the-final-search)).
