# S5 — FM phase modulator (`fm_phase_modulator`)

## Physical process

The pre-emphasised video frequency-modulates the main carrier. The
modulator's output is a carrier whose instantaneous frequency follows the
video level.

## Sources

- IEC 60857-1986 §9.2.1: FM, positive modulation.
- §9.2.2: blanking (0 IRE) → 8.1 MHz ± 50 kHz.
- §9.2.3: sync tip (−40 IRE) to white (+100 IRE) deviation 1.7 MHz ± 35 kHz;
  white is the highest frequency.

## Model and equations

**Frequency law.** §9.2.1 makes the modulation linear FM with positive
polarity. §9.2.2 and §9.2.3 fix two points of the straight line: 0 IRE →
f_b, and a 140 IRE span → Δf_dev. So

```
f(t) = f_b + k · v(t),    k = Δf_dev / 140 IRE
```

with nominal f_b = 8.1 MHz, Δf_dev = 1.7 MHz and k = 12 142.857 Hz/IRE.
`v(t)` is the D2 signal in IRE, after S3 and S4.

| Level | IRE | Carrier |
|-------|-----|---------|
| Sync tip | −40 | 7.614 286 MHz |
| Blanking | 0 | 8.100 000 MHz |
| Black (setup) | 7.5 | 8.191 071 MHz |
| White | 100 | 9.314 286 MHz |
| Peak (code 1019) | 139.1 | 9.789 158 MHz |

Pre-emphasis overshoot extends the instantaneous range to about 4.8–12.2 MHz
for the largest steps ([S4](s04-video-pre-emphasis.md)).

**Phase.** The carrier phase is the integral of frequency:

```
φ(t) = φ₀ + 2π ∫₀ᵗ f(u) du
```

S5 outputs φ, not cos φ. Every later stage needs the times at which φ
crosses given values (S7), and these are found far more accurately from φ
than from a sampled cosine.

The requirement comes from the S7 edge-time budget: the phase error must
stay below `2π · f · 4 ps`, which is 3 × 10⁻⁴ rad at 12.2 MHz, over a whole
run.

**Integration rule (plan Task 3.4).** The increment over one Fm interval is
the integral of the degree-5 polynomial through the six samples around it:

```
φ(t_{m+1}) − φ(t_m) = 2π·h · Σⱼ cⱼ · f(t_{m−2+j}),   c = (11, −93, 802, 802, −93, 11) / 1440
```

with h = 1/Fm. The weights integrate the Lagrange polynomial through
m−2 … m+3 over [m, m+1] (exact for degree ≤ 5). For a component e^{jωt} the
rule's response divided by the exact integral is real (the rule is
symmetric about the interval's centre), so it adds no phase. Its gain error
is:

| Frequency | Gain error |
|-----------|------------|
| 0.5 MHz | 8 × 10⁻¹² |
| fsc | 1.0 × 10⁻⁶ |
| 4.2 MHz | 2.6 × 10⁻⁶ |
| 6 MHz | 2.2 × 10⁻⁵ |

That is a gain error of at most 0.0002 dB in the recorded video, well inside
the S2 passband bound. A constant input gives the same increment every
interval, so a constant level gives an exact constant frequency.

**Accumulation.** The phase is accumulated in turns as a whole number of
cycles (64-bit integer) plus a fraction in [0, 1). So the rounding error of
the sum stays at the level of one addition to a number below 1 (10⁻¹⁶ turns)
and does not grow with the run. The output φ = φ₀ + 2π·(cycles + fraction)
is rounded once, to half an ulp of φ: over a full side (3600 s,
φ ≈ 1.8 × 10¹¹ rad) that is 1.5 × 10⁻⁵ rad, or 0.3 ps at 8.1 MHz.

**Phase between samples.** D3 carries f(t_m) as well as φ(t_m). Between
samples, φ is the cubic Hermite interpolant on φ and its slope 2πf. Its
error bound, at most 1.1 × 10⁻⁴ rad, is derived in
[S2](s02-video-rate-converter.md), where it sets Fm. S7 and the carrier
diagnostic both use this interpolant.

The input is 0 IRE outside the run (as S2), so f = f_b there.

## Parameters

| Register row | Value |
|--------------|-------|
| `S5.blanking_frequency` | 8.1 MHz ± 50 kHz |
| `S5.deviation` | 1.7 MHz ± 35 kHz (sync tip → white) |
| `S5.deviation_span` | 140 IRE (fixed: −40 to +100) |
| `S5.initial_phase` | 0 rad |
| `S5.integration_rule` | (11, −93, 802, 802, −93, 11)/1440 |
| `S5.phase_interpolant` | Cubic Hermite on φ and 2πf |

## Assumptions and approximations

- The modulator is perfectly linear. §9.2 defines no linearity tolerance; a
  nonlinear modulator would need a source.
- The initial phase φ₀ is a declared choice. Any value gives a valid disc; 0
  is used so the value is documented, and the key exists so a test can show
  the decoded output does not depend on it.

## Input/output contract

- **In:** D2 (IRE at `Fm`).
- **Out:** D3 — φ(t_m) in radians and f(t_m) in Hz at `Fm`, same time
  origin, with the slice level *s* ≡ 0 (filled by S6). For M D2 samples
  there are M + 1 D3 samples: the last is at the end of the run, so the
  interpolant covers every instant of it.

## Project YAML block

`fm_phase_modulator`:

| Key | Type | Default | Range | IEC tolerance |
|-----|------|---------|-------|---------------|
| `blanking_hz` | float | 8 100 000 | 6 000 000–10 000 000 | 8 050 000–8 150 000 |
| `deviation_hz` | float | 1 700 000 | 500 000–3 000 000 | 1 665 000–1 735 000 |
| `initial_phase_rad` | float | 0.0 | −π to π | — |

## Ideal setting

The nominal frequencies, φ₀ = 0.

## Verification

`tests/unit/test_fm_phase_modulator.cpp`:

- constant −40, 0 and 100 IRE inputs give 7.614 286, 8.1 and 9.314 286 MHz,
  measured from the slope of φ, within 1 Hz (plan Task 3.4);
- φ starts at φ₀, and *s* is 0;
- a 4.2 MHz, 50 IRE modulation matches the analytic phase within 10⁻⁶ rad;
- output identical across block sizes.

## Diagnostic tap

`fm_phase_modulator`: φ as `float64` with a descriptor. The carrier
diagnostic mode (`output.mode: carrier_diagnostic`) renders `cos φ` from this
representation directly (plan Task 3.5).

## Open items

None.
