# S4 — Video pre-emphasis (`video_pre_emphasis`)

## Physical process

Before frequency modulation, the mastering chain boosts the high video
frequencies. The player applies the inverse (de-emphasis) after
demodulation, which lowers the high-frequency noise that FM demodulation
produces. Pre-emphasis is part of the recorded signal, so the disc carries it.

## Sources

- IEC 60857-1986 §9.2.4 and Figure 9:
  `A = V_out / V_in = (1 + jωt₁) / (1 + jωt₂)`, with t₁ = 320 ± 8 ns and
  t₂ = 120 ± 2 ns.

## Model and equations

The stage is the linear, time-invariant filter

```
H(s) = (1 + s·t₁) / (1 + s·t₂)
```

Derived properties, at nominal t₁ and t₂:

| Property | Value | Derivation |
|----------|-------|------------|
| DC gain | 1 (0 dB) | H(0) |
| High-frequency gain | t₁/t₂ = 2.667 (+8.52 dB) | H(∞) |
| Zero | 1/(2πt₁) = 497.4 kHz | |
| Pole | 1/(2πt₂) = 1.326 MHz | |
| Gain at fsc (3.579545 MHz) | 2.525 (+8.04 dB) | \|H(j2πf)\| |
| Phase at fsc | +12.4° | arg H |
| Group delay | +200 ns at DC; +54.1 ns at 0.5 MHz; −8.4 ns at fsc; −6.5 ns at 4.2 MHz | `t₁/(1+(ωt₁)²) − t₂/(1+(ωt₂)²)` |

The discretisation method (for example a bilinear transform with
pre-warping, or a design matched to the analogue response over the band) is
chosen in plan Task 3.3. Its requirement is a magnitude within 0.05 dB of
`|H(jω)|` from 0.1 to 6 MHz, with DC gain exactly 1.

**Deviation and pre-emphasis (Task 1.6 question 1).** The question was
whether the §9.2.3 deviation is set before or after pre-emphasis. Since
H(0) = 1, a constant input level leaves the stage unchanged. The deviation is
defined by constant levels (sync tip and white), so it is the same at either
point, and the question does not arise. Transients are another matter: they
overshoot (below).

**Overshoot (Task 1.6 question 2).** A step of height Δ (IRE) first appears
at the output with height `Δ·t₁/t₂ = 2.67Δ`, then relaxes with time constant
t₂ to Δ. For the largest steps a video signal contains, with the S5 mapping:

| Step | Initial peak | Instantaneous carrier |
|------|--------------|-----------------------|
| white (100) → sync tip (−40) | −273 IRE | 4.78 MHz |
| sync tip (−40) → white (100) | +333 IRE | 12.15 MHz |

These are the limits for an instantaneous step. A 4fsc-sampled input is
band-limited to 7.16 MHz, so its real overshoot is smaller. The carrier stays
positive in every case, so the FM modulator needs no limiting.

**Decision: no clipper.** §9.2.4 specifies a linear transfer function and no
source documents a clipper in LaserVision mastering. A clipper would add a
nonlinearity that no source supports, so the ideal disc has none. A clipper
is added later only if a source documents one, as an impairment that is off
by default.

## Parameters

| Register row | Value |
|--------------|-------|
| `S4.t1` | 320 ± 8 ns |
| `S4.t2` | 120 ± 2 ns |

## Assumptions and approximations

- The analogue pre-emphasis network matched §9.2.4 exactly. Departures within
  tolerance are modelled by setting t₁ and t₂.
- The discretisation error is the Task 3.3 bound (0.05 dB, 0.1–6 MHz).

## Input/output contract

- **In/out:** D2, same time origin. The stage's own group delay is part of
  the specified response and is **not** removed (unlike the bulk delay of
  S3): the player's de-emphasis cancels it.

## Project YAML block

`video_pre_emphasis`:

| Key | Type | Default | Range | IEC tolerance |
|-----|------|---------|-------|---------------|
| `t1_ns` | float | 320 | 50–1000 | 312–328 |
| `t2_ns` | float | 120 | 20–500 | 118–122 |

`t1_ns` must be greater than `t2_ns`.

## Ideal setting

Nominal t₁ = 320 ns and t₂ = 120 ns. There is no bypass: a disc without
pre-emphasis is not a LaserVision disc.

## Verification

Unit tests compare the realised magnitude with `|H(jω)|` from 0.1 to 6 MHz
(within 0.05 dB) and check DC gain 1. A step response test checks the initial
peak ratio against t₁/t₂, allowing for the input band limit.

## Diagnostic tap

`video_pre_emphasis`: D2 after the stage.

## Open items

None.
