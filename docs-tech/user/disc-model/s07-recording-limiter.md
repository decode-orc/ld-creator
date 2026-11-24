# S7 — Recording limiter (`recording_limiter`)

## Physical process

The cut is binary: at any point on the track there is either a pit or land.
So the recorder drives its laser with a **limited** (two-level) version of
the modulated carrier, and the information is carried entirely by the times
at which the level changes.

## Sources

- IEC 60857-1986 §7.2.1: pulse-width modulation "on the main carrier" needs a
  two-level carrier whose pulse widths can be modulated.
- BBC RD 1986/01 §1.2: video modulates pit spacing, and sound modulates the
  ratio of pit length to the space between pits.
- IEC 60857-1986 Amendment 1, Figure 21: pits drawn as discrete depressions
  in a flat information surface.
- Philips Technical Review 33 (1973) p. 179 (Compaan & Kramer): the laser
  recording is a "highly non-linear" photographic process, so the recording is
  two-level only; p. 181: on average a pit occupies exactly half the track.
- F. N. Fritsch and R. E. Carlson, "Monotone piecewise cubic
  interpolation", *SIAM J. Numer. Anal.* 17(2) 238–246 (1980): the slope
  condition under which a cubic Hermite segment is monotone (root finding,
  below).

## Model and equations

Pit-edge times are the solutions of

```
cos φ(t) = s(t)
```

With *s* ≡ 0 (video only) the solutions are φ(t_k) = (k + ½)π. The pit is
present while `cos φ(t) > s(t)`.

**Polarity (declared decision).** Which half-cycle becomes a pit is not
stated by any source. FM demodulation is unaffected by inverting the signal,
so the choice cannot change the decoded video. The pit is assigned to
`cos φ > s`, and the choice is recorded here so it is not implicit.

**Edge-time budget.** The edge times are the recorded information, so the
error in finding them must be negligible against the round-trip tolerance.
An error δt in one edge changes the length of one half-cycle, and so the
frequency estimated from that half-cycle by

```
δf = 2 f² · δt
```

Requiring δf below 0.1 IRE (1 214 Hz), which is a tenth of the ±1 IRE
round-trip level threshold ([metrics](../ntsc-round-trip-metrics.md)), gives

| Carrier | Allowed δt |
|---------|-----------|
| 8.1 MHz (blanking) | 9.3 ps |
| 9.31 MHz (white) | 7.0 ps |
| 12.15 MHz (largest overshoot) | 4.1 ps |

The budget for S2 + S5 + S7 together is therefore **4 ps** per edge. A
demodulator averages over several half-cycles, so this bound is conservative.

**Root finding.** Within each `Fm` interval, φ is the cubic Hermite
interpolant on φ and 2πf ([S5](s05-fm-phase-modulator.md)). Fm was chosen
so that this interpolant's error is at most 1.45 ps of edge time, 36 % of
the budget ([S2](s02-video-rate-converter.md)). Each crossing is found by
bracketing the target phase between samples and solving on the
interpolant:

- **Bracketing.** Crossing k belongs to the interval [t_m, t_m+1] for which
  φ_m < (k + ½)π ≤ φ_m+1. The target is always evaluated as
  `(k + ½)·π` in double precision, so a crossing that falls exactly on a
  sample is assigned to exactly one interval.
- **Monotonicity.** A cubic Hermite segment is monotone when both end
  slopes lie between 0 and 3 times the secant (Fritsch & Carlson, *SIAM J.
  Numer. Anal.* 17 (1980) 238–246). Then each target is crossed exactly once
  in its interval. The stage checks this in every interval and stops with an
  error if it fails. At Fm the carrier frequency changes by well under 1 %
  per interval, so the ratios are close to 1.
- **Solver.** Newton's method on `φ(u) − φ_m = (k + ½)π − φ_m`, u ∈ [0, 1],
  from the linear estimate. The bracket narrows with each iterate, and a
  step that would leave it is replaced by bisection. The solver stops when a
  step is below 2ε in u (2.6 × 10⁻²⁴ s), or after 64 iterations. The
  interpolant is nearly linear, so Newton converges in 3–4 steps.
- **Time.** `t_k = (m + u) / Fm`.

**Edge-time error.** For a run of up to one hour (a full CLV side), at the
12.15 MHz worst-case carrier:

| Source | Bound |
|--------|-------|
| Hermite interpolant ([S2](s02-video-rate-converter.md)) | 1.45 ps |
| D3 phase and target rounding: 2 × ½ ulp of φ ≈ 1.8 × 10¹¹ rad (3.05 × 10⁻⁵ rad) | 0.40 ps |
| Edge time rounding: ½ ulp of 3600 s | 0.23 ps |
| Solver | < 10⁻²³ s |
| **Total** | **2.1 ps**, inside the 4 ps budget |

Measured on a linear frequency sweep, where the interpolant is exact, the
worst edge differs from the analytic root by 4 × 10⁻¹⁹ s. On a 9 MHz carrier
with ±2 MHz sinusoidal FM at 1 MHz, the worst is 2.7 fs. The 1.45 ps S2 bound
is a worst case over the whole video band, which these signals do not reach.

**Slice level.** Only *s* ≡ 0 is implemented. The stage rejects a non-zero
*s*, so that audio or EFM cannot reach it before the `cos φ = s` solver
exists.

## Parameters

| Register row | Value |
|--------------|-------|
| `S7.edge_budget` | 4 ps (derived) |
| `S7.pit_polarity` | pit where cos φ > s (declared) |
| `S7.root_solver` | Newton in the bracket, bisection fallback; 2ε step, 64 iterations |
| `S7.monotone_slope_ratio` | 3 (Fritsch & Carlson 1980) |

## Assumptions and approximations

- An ideal limiter: zero rise time and no hysteresis. The physical rise time
  of the cut appears as the cutting-spot exposure profile in S8, where it
  belongs, not here.

## Input/output contract

- **In:** D3 (φ and *s* at `Fm`).
- **Out:** D4 — ordered edge times `t_k` (`double`, seconds from the D1
  origin), with polarity alternating pit-start / pit-end.

## Project YAML block

None. The limiter has no settings: its threshold is *s*(t) from S6, and the
duty-cycle departures of a real cut are an S8 exposure effect.

## Ideal setting

The stage is always the ideal limiter.

## Verification

Unit tests (`tests/unit/test_recording_limiter.cpp`, plan Task 4.2):

- On a constant carrier at 7.614, 8.1, 9.314 and 12.15 MHz, edge intervals
  equal 1/(2f) within the 4 ps budget, and polarities alternate. The same
  holds for S5's output at −40, 0 and 100 IRE.
- On a linear sweep, every edge is within the budget of the analytic root.
- From φ = 0 the first edge is a pit end, at φ = π/2.
- The output is identical for block sizes 1, 2, 7, 4096 and the whole run.
- A non-zero slice level and a phase that does not increase are rejected.

**Independent check.** `scripts/verify_stages.py` (plan Task 6.2) checks the taps of real runs with tools written independently of the implementation ([scripts/README.md](../../../scripts/README.md)). The reference edges are the roots of
φ(t) = (k + ½)π found from the D3 tap alone, on the degree-5 Lagrange
polynomial through the six φ samples around each bracket (no use of f or
of the Hermite interpolant). One frame, at each of CAV 55, 80, 110 and
144.99 mm and CLV 10.1 and 11.4 m/s:

| Quantity | Result | Limit |
|----------|--------|-------|
| Edges up to the last φ sample of the frame | 567 056, as the bracket rule predicts | exact |
| Polarity | pit end at φ = (2n + ½)π, pit start at (2n + 1½)π, every edge | exact |
| Largest edge-time difference | 0.272 ps (rms 0.006 ps) | 4 ps (`S7.edge_budget`) |

The check excludes the first and last 1 µs of the run. There the input steps
to 0 IRE outside the run, and the φ-only polynomial reference (not the
stage) loses accuracy: up to 3.8 ps within 0.3 µs of the end. One edge per
frame falls after the tap's last whole Fm sample and cannot be checked from
it.

## Diagnostic tap

`recording_limiter`: D4 as binary records (`float64` time, `int8` polarity)
with a descriptor.

## Open items

None.
