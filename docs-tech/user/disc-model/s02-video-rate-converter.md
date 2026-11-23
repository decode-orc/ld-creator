# S2 — Video rate converter (`video_rate_converter`)

## Physical process

None on the real disc: mastering electronics are analogue, and the signal is
continuous. This stage exists because the model represents that continuous
signal by samples at a **modelling rate** `Fm` that is high enough for S3–S7.
Its only job is to add no error of its own beyond a stated bound.

## Sources

- Sampling theorem: D1 is a 4fsc sampling of a signal band-limited below
  2fsc ≈ 7.16 MHz, so it determines a unique band-limited continuous signal.
- S5 and S7 notes, which set the accuracy that `Fm` must support.

## Model and equations

`x(t)` is the band-limited signal defined by the D1 samples. S2 produces
`x(t_m)` at `t_m = m / Fm` (same time origin as D1) by band-limited
interpolation. The interpolator's design (kernel, length, window) is chosen in
plan Task 3.1, together with `Fm`, against these requirements:

1. **Passband:** magnitude error below 0.01 dB and group-delay error below
   0.1 ns from 0 to 6 MHz. This is a factor of 10 inside the S3 and S4
   acceptance tolerances (±0.1 dB and the §9.1.7 delays), so S2 never
   dominates them.
2. **Edge timing:** the edge-time error that S2 adds, after S5 and S7, is
   inside the 4 ps budget derived in [S7](s07-recording-limiter.md).

`Fm` is a fixed implementation constant, recorded in the register when Task
3.1 sets it, and stamped in the resolved project through the ld-creator
version. It is not a project key: changing it is a change to the model, not a
setting.

## Parameters

| Register row | Value |
|--------------|-------|
| `S2.modelling_rate` | Set by Task 3.1 (open) |
| `S2.passband_error` | ≤ 0.01 dB, ≤ 0.1 ns, 0–6 MHz |

## Assumptions and approximations

- The 4fsc input has no content above 2fsc. videosynth generates at 4fsc, so
  this holds by construction.
- The finite interpolation kernel is the only error source. Its bound is the
  passband requirement above.

## Input/output contract

- **In:** D1 (IRE at 4fsc).
- **Out:** D2 (IRE at `Fm`), same time origin. Output sample 0 is at `t = 0`.

## Project YAML block

None. The stage has no settings.

## Ideal setting

The stage is always the ideal resampler.

## Verification

Unit tests at 0.5, 3.58 and 4.2 MHz: sinusoids at 4fsc, converted to `Fm`,
match the analytic signal within the passband bound.

## Diagnostic tap

`video_rate_converter`: D2 as `float64` with a descriptor.

## Open items

- The value of `Fm` and the kernel (plan Task 3.1).
