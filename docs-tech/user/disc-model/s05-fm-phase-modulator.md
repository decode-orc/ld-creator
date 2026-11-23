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

The integration rule is chosen in plan Task 3.4. The requirement comes from
the S7 edge-time budget: the phase error must stay below `2π · f · 4 ps`,
which is 3 × 10⁻⁴ rad at 12.2 MHz, over a whole run. The phase is accumulated
in `double`, and over a full side (3600 s, φ ≈ 2 × 10¹¹ rad) the rounding
step of φ is about 3 × 10⁻⁵ rad, inside the budget. The implementation keeps
whole cycles separately if the measured error approaches the budget.

## Parameters

| Register row | Value |
|--------------|-------|
| `S5.blanking_frequency` | 8.1 MHz ± 50 kHz |
| `S5.deviation` | 1.7 MHz ± 35 kHz (sync tip → white) |
| `S5.deviation_span` | 140 IRE (fixed: −40 to +100) |
| `S5.initial_phase` | 0 rad |

## Assumptions and approximations

- The modulator is perfectly linear. §9.2 defines no linearity tolerance; a
  nonlinear modulator would need a source.
- The initial phase φ₀ is a declared choice. Any value gives a valid disc; 0
  is used so the value is documented, and the key exists so a test can show
  the decoded output does not depend on it.

## Input/output contract

- **In:** D2 (IRE at `Fm`).
- **Out:** D3 — φ(t_m) in radians at `Fm`, same time origin, with the slice
  level *s* ≡ 0 (filled by S6).

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

Unit tests (plan Task 3.4): constant −40, 0 and 100 IRE inputs give
7.614 286, 8.1 and 9.314 286 MHz, measured from the slope of φ, within 1 Hz.
Output is identical across block sizes.

## Diagnostic tap

`fm_phase_modulator`: φ as `float64` with a descriptor. The carrier
diagnostic mode (`output.mode: carrier_diagnostic`) renders `cos φ` from this
representation directly (plan Task 3.5).

## Open items

None.
