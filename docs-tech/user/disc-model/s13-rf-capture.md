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
  steps (200 mV p-p input, gain switch "1000"); sheet 2, the filter design's
  predicted phase, 0–16 MHz in 250 kHz steps; sheet 3, the gain table.
- `docs/content/development/hardware-guide.md`: input stage, gain theory,
  filter stage, ADC and FPGA clocking.
- `docs/content/capture-gui/capture-files.md`: sample format of captures.
- `ddd-gui/tests/unit/test_sample_format.cpp`: the 16-bit representation of
  a 10-bit code.

## Model and equations

The chain, in signal order:

| Block | Components | Model | Source |
|-------|------------|-------|--------|
| Input termination | R401 56 Ω ∥ bias divider (R402, R403 1 kΩ to the ADC references → 500 Ω) | 50.36 Ω load | Hardware guide, "Input impedance" |
| AC coupling | C401 100 nF into the 500 Ω divider | First-order high-pass, `1/(2π · 500 Ω · 100 nF)` = 3.18 kHz | Schematic |
| Amplifier | OPA690, non-inverting, gain `1 + A/B`, B = R405 = 200 Ω, A = the switched parallel combination of R406–R409 (1.5 k, 1 k, 680, 560 Ω) | Gain 2.02–8.5 in 15 settings; flat to beyond 100 MHz | Hardware guide, "Operational amplifier gain theory"; sheet 3 |
| Anti-alias filter | L401 680 nH, L402 560 nH, C403 330 pF, C404 33 pF, C405 470 pF, C406 100 pF, C407 270 pF (5 %), R410 and R411 47 Ω terminations | Passive LC elliptic low-pass; transfer function computed from the netlist | Schematic; hardware guide, "Filter stage" |
| ADC | ADS825, 10-bit, 2 V p-p full scale centred on 2.5 V | Sampling at 40 MHz, quantisation to codes 0–1023 | Hardware guide, "ADC"; schematic |
| Clock | 80 MHz FPGA PLL from the DE0-Nano 50 MHz oscillator, divided by 2 | Exact 40 MHz at the ideal setting | Hardware guide, "DE0-Nano FPGA" |

**Filter model (decision).** The filter's transfer function is computed from
the netlist with nominal component values (plan Task 5.3). This is a
derivation from cited quantities. It is then checked against the measured
response (sheet 1), which includes the input stage and amplifier. The guide
reports −3 dB at about 13.8 MHz and less than −35 dB at 20 MHz; sheet 1, taken
relative to its 1 MHz value, gives −3.3 dB at 12.1 MHz and −28.4 dB at
17.8 MHz. The filter's group delay is modelled, not only its magnitude: the
DdD documentation itself notes that delay that varies with frequency
disperses an FM signal (`fpga-decimation-filter.md`, "Why this filter").

**Sampling.** D9 is at Fr = 80 MHz (S11). After the continuous-time chain
(realised at Fr), the ADC samples every second value:

```
D10[n] = Q( level · y(τ = n / 40 MHz) ),   sample 0 at τ = 0
```

Taking every second sample is exact because D9 is band-limited below 40 MHz
(S11), and the anti-alias filter has already removed the content above
20 MHz that would otherwise alias into the capture, as the real hardware
does.

**Level (decision).** The DdD guide sets the working level at 75 % of the
converter's 2 V p-p window ("Aim for (75%)"). The model sets `level` so
that the peak-to-peak amplitude of the **blanking-level carrier** (8.1 MHz,
50 % duty) at the run's start radius, after S11–S13's filtering (including
the S12 MTF equaliser when it is on), is
`level_fraction` × full scale. This reference is computed analytically from
the S11 model, so it needs no second pass over the data. Signals with more
modulation depth than the reference (lower carrier frequencies, which the
readout attenuates less) use the headroom above 75 %. Clipped samples are
counted and reported, as the DdD capture application does.

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

The filter design itself is chosen in plan Task 5.3 against these numbers.

`round` is to the nearest integer, halves away from zero, in both profiles.
Both quantisers are implemented (`src/rf_capture/capture_quantiser.cpp`);
the anti-alias filters and the level reference are plan Task 5.3.

### Carrier diagnostic

`output.mode: carrier_diagnostic` (plan Task 3.5) bypasses S6–S12 and this
stage's filter, but uses its sampling grid and quantiser. Capture sample n,
at τ = n / 40 MHz, falls at n·189/88 Fm samples, and u = level_fraction ·
cos φ(τ). The carrier has a constant amplitude, so its peak-to-peak is
exactly level_fraction of full scale, which is the level rule above applied to
it. It is not a disc.

## Parameters

| Register row | Value |
|--------------|-------|
| `S13.profile` | `domesday_duplicator` |
| `S13.sample_rate` | 40 MHz (fixed) |
| `S13.adc_bits` | 10 (DdD), 16 (`ideal`) |
| `S13.quantiser`, `S13.ideal_quantiser` | The two Q rules above |
| `S13.adc_full_scale` | 2 V p-p |
| `S13.level_fraction` | 0.75 |
| `S13.filter_components` | schematic values above |
| `S13.ac_coupling_corner` | 3.18 kHz |

## Assumptions and approximations

- **Noiseless.** The ADC clock interference the guide describes, ADC noise
  and amplifier noise are Phase 9 work.
- **Amplifier gain is flat.** The OPA690 bandwidth (> 100 MHz) is well above
  the filter. Because the level is set by `level_fraction`, the gain-switch
  setting has no effect on a noiseless capture, so it is not a key in this
  scope.
- **Nominal components.** The 5 % capacitor tolerance moves the filter
  response; plan Task 5.3 states the resulting deviation from sheet 1.
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

Plan Task 5.3: the computed filter response matches sheet 1 within the
component-tolerance bound; above 20 MHz the response is down by the computed
amount; no clipping on any test pattern at the default level.

## Diagnostic tap

`rf_capture`: the continuous-time output `y` at Fr before sampling, as
`float64`.

## Open items

- Netlist extraction of the filter topology from `RF Amplifier.kicad_sch`
  (plan Task 5.3).
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
