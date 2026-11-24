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
interpolation, against two requirements:

1. **Passband:** magnitude error below 0.01 dB and group-delay error below
   0.1 ns from 0 to 6 MHz. This is a factor of 10 inside the S3 and S4
   acceptance tolerances (±0.1 dB and the §9.1.7 delays), so S2 never
   dominates them.
2. **Edge timing:** the edge-time error that S2, S5 and S7 add together is
   inside the 4 ps budget derived in [S7](s07-recording-limiter.md).

### The modelling rate

**Fm = 24 fsc = 6 × 4fsc = 945/11 MHz ≈ 85.909 MHz**, so there are exactly 6
D2 samples per D1 sample and 2 866 500 per frame.

The rate is set by requirement 2. S7 finds pit edges, and the carrier
diagnostic renders the carrier, from φ between D3 samples. D3 carries φ and
its slope 2πf at every sample (S5), and φ between them is the **cubic
Hermite interpolant** of those values. Its error on an interval of length
h = 1/Fm is at most (de Boor, *A Practical Guide to Splines*, ch. II)

```
|φ(t) − φ_H(t)| ≤ h⁴/384 · max |φ⁗|
```

φ⁗ = 2π·Δf‴, where Δf(t) = f(t) − f_b = k·v(t) is the frequency deviation.
Δf is band-limited to B = 2fsc = 7.16 MHz, because the D1 input is a 4fsc
sampling and S3–S4 are linear filters. For video between sync tip and white, its
largest value is the pre-emphasis overshoot of a sync-to-white step,
4.05 MHz (12.15 MHz − 8.1 MHz; see [S4](s04-video-pre-emphasis.md)). Bernstein's inequality for a signal
band-limited to B gives |Δf‴| ≤ (2πB)³·max|Δf|, so

```
max |φ⁗| ≤ 2π · (2π · 7.16 MHz)³ · 4.05 MHz = 2.32 × 10³⁰ rad/s⁴
```

| Fm | Phase error bound | Edge-time error at 12.15 MHz | Share of the 4 ps budget |
|----|-------------------|------------------------------|--------------------------|
| 4 × 4fsc = 57.27 MHz | 5.6 × 10⁻⁴ rad | 7.4 ps | 184 % |
| 5 × 4fsc = 71.59 MHz | 2.3 × 10⁻⁴ rad | 3.0 ps | 75 % |
| **6 × 4fsc = 85.91 MHz** | **1.1 × 10⁻⁴ rad** | **1.45 ps** | **36 %** |

The edge-time error is δφ/(2πf). 6 × 4fsc is the lowest integer multiple of
4fsc that leaves more than half of the budget for S2's own error and S5's
integration and rounding, which are below. The bound is the worst case the
4fsc input allows. For video band-limited to 4.2 MHz (SMPTE 170M), it is a
factor of (4.2/7.16)³ = 0.2 smaller. The §9.2.3 deviation tolerance,
±35 kHz (2.9 IRE), is 29 times looser than the 0.1 IRE behind the edge
budget, so the edge budget is the requirement that sets Fm.

An integer multiple of 4fsc keeps the time grids exactly rational:
output sample m sits at u_m = (15m − 57)/90 input samples, so the
interpolator has 6 fixed phases. Fm/40 MHz = 189/88 is also exact, so the
capture grid is (S13 and the carrier diagnostic).

### The interpolation kernel

A Kaiser-windowed sinc with its cut-off at the input Nyquist frequency 2fsc:

```
k(x) = sinc(x) · I₀(β·√(1 − (x/20)²)) / I₀(β),   |x| ≤ 20,   β = 9
```

x is in input sample periods (Kaiser, 1974). Output sample m is
Σⱼ k(u_m − n_j)·x[n_j] over the 40 input samples nearest u_m. Each phase's
40 weights are divided by their sum, so the DC gain is 1 to rounding. sin
and I₀ are computed from basic arithmetic only (`src/dsp/`), so the weights
are the same on every host. Samples outside the run are taken as 0 IRE.
That is blanking, the level that ends line 525 before line 1.

Measured responses of the 6 phases, and of the combined interpolator for its
images:

| Property | Requirement | Realised |
|----------|-------------|----------|
| Magnitude, 0–6 MHz | ≤ 0.01 dB | ≤ 0.0005 dB |
| Group delay, 0–6 MHz | ≤ 0.1 ns | ≤ 0.079 ns |
| Images, 8.32 MHz (4fsc − 6 MHz) upwards | — | ≥ 91.5 dB down |

An image 91.5 dB below a 140 IRE signal is 0.004 IRE. After the
pre-emphasis gain of at most 2.67 it shifts the carrier by at most 120 Hz,
a tenth of the 1 214 Hz behind the edge budget.

Cost: 40 multiply-adds per output sample.

## Assumptions and approximations

- The 4fsc input has no content above 2fsc. videosynth generates at 4fsc, so
  this holds by construction. Content between 6 MHz and 2fsc is attenuated
  by the kernel's transition band; NTSC video has none (SMPTE 170M, 4.2 MHz).
- The finite interpolation kernel is the only error source. Its realised
  error is in the table above.

## Input/output contract

- **In:** D1 (IRE at 4fsc).
- **Out:** D2 (IRE at `Fm`), same time origin. Output sample 0 is at `t = 0`;
  there are exactly 6 output samples per input sample.

## Project YAML block

None. The stage has no settings.

## Ideal setting

The stage is always the ideal resampler.

## Verification

`tests/unit/test_video_rate_converter.cpp`:

- sinusoids at 0.5, 3.58 and 4.2 MHz at 4fsc, converted to `Fm`, fitted
  by least squares: gain within 0.01 dB, delay within 0.1 ns, and residual
  below 10⁻⁴ of the amplitude (plan Task 3.1);
- unit DC gain of every phase, and a constant level reproduced;
- identical output for block sizes from 1 sample upwards.

## Diagnostic tap

`video_rate_converter`: D2 as `float64` with a descriptor.

## Open items

None.
