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
| Group delay | −200 ns at DC; −54.1 ns at 0.5 MHz; +8.4 ns at fsc; +6.5 ns at 4.2 MHz: a lead network advances low frequencies | `−d(arg H)/dω = t₂/(1+(ωt₂)²) − t₁/(1+(ωt₁)²)` |

**Discretisation (plan Task 3.3).** The requirement is a magnitude within
0.05 dB of `|H(jω)|` from 0.1 to 6 MHz, with DC gain exactly 1. H is
written as a unit path plus a high-pass branch,

```
H(s) = 1 + (t₁/t₂ − 1) · s·t₂ / (1 + s·t₂)
```

and the branch is discretised by the bilinear transform
s = 2Fm·(1 − z⁻¹)/(1 + z⁻¹), without pre-warping. With K = 2Fm:

```
hp[m] = g·(x[m] − x[m−1]) − p·hp[m−1],   g = K·t₂/(1 + K·t₂),   p = (1 − K·t₂)/(1 + K·t₂)
y[m]  = x[m] + (t₁/t₂ − 1)·hp[m]
```

This is exactly the bilinear transform of H(s), since the transform is a
substitution and so preserves the sum. The branch sees only differences of
the input, so a constant input passes unchanged in floating point too: the
DC gain is exactly 1, not just to rounding.

The bilinear transform maps the digital frequency f onto the analogue
frequency (Fm/π)·tan(πf/Fm). That is 6.097 MHz at f = 6 MHz, a 1.6 %
warp. Because |H| is nearly flat there, the realised magnitude is within
**0.0055 dB** of `|H(jω)|` over 0.1–6 MHz (largest at 6 MHz), a ninth of
the requirement, so pre-warping is not needed.

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
| `S4.discretisation` | Bilinear transform at Fm, DC gain exactly 1 |

## Assumptions and approximations

- The analogue pre-emphasis network matched §9.2.4 exactly. Departures within
  tolerance are modelled by setting t₁ and t₂.
- The discretisation error is the Task 3.3 bound (0.05 dB, 0.1–6 MHz);
  realised: 0.0055 dB.
- The filter starts at rest, with the input 0 IRE before the run (as S2).

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

`tests/unit/test_video_pre_emphasis.cpp`:

- realised magnitude within 0.05 dB of `|H(jω)|` every 10 kHz from 0.1 to
  6 MHz (plan Task 3.3), and the derived +8.04 dB at fsc;
- a constant level passes exactly;
- a step's first output sample is 1 + (t₁/t₂ − 1)·g = 2.590 times the
  step: within 3 % of t₁/t₂ = 2.667, the difference being the bandwidth of
  a one-sample step at Fm;
- a streamed subcarrier has the designed gain; output identical for any
  block size.

**Independent check.** `scripts/verify_stages.py` (plan Task 6.2) checks the taps of real runs with tools written independently of the implementation ([scripts/README.md](../../../scripts/README.md)). H = P_xy/P_xx between the
`group_delay_predistorter` and `video_pre_emphasis` taps of one frame (a
photograph with NTC-7 and FCC test lines), against the analytic
(1 + s·t₁)/(1 + s·t₂):

| f (MHz) | Analytic | Magnitude error | Phase error | Group delay | Analytic delay |
|---------|----------|-----------------|-------------|-------------|----------------|
| 0.1 | +0.147 dB | +0.0010 dB | +0.002° | −188.1 ns | −188.2 ns |
| 0.5 | +2.456 dB | +0.0011 dB | −0.001° | −54.6 ns | −54.1 ns |
| 1 | +5.072 dB | +0.0053 dB | −0.008° | +12.8 ns | +13.1 ns |
| 2 | +7.197 dB | +0.0044 dB | −0.027° | +18.1 ns | +18.0 ns |
| 3.58 | +8.044 dB | +0.0052 dB | −0.064° | +8.45 ns | +8.42 ns |
| 4.2 | +8.167 dB | +0.0054 dB | −0.079° | +6.52 ns | +6.46 ns |
| 6 | +8.342 dB | +0.0057 dB | −0.123° | +3.46 ns | +3.41 ns |

Every magnitude is within the 0.05 dB limit; the largest, 0.0057 dB at
6 MHz, matches the 0.0055 dB bilinear warp derived above (the input is
74 dB below its peak there, so the estimate is still exact in a noiseless
run). The phase error grows with frequency as the warp predicts.

This check found a sign error in this note: the group-delay row of the
first table gave `+d(arg H)/dω` as the group delay. A lead network advances
low frequencies, so the delay is −200 ns at DC, as now stated. The
implementation was not affected; S3's note repeated the error and is
corrected too.

## Diagnostic tap

`video_pre_emphasis`: D2 after the stage.

## Open items

- **Resolved (plan Task 6.2):** the group-delay row of the derived-property
  table had the wrong sign. The independent check (Verification) measured
  −200 ns at DC, and the row now gives −d(arg H)/dω. No code used the
  values.
