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

**Root finding.** Within each `Fm` interval, φ is represented by a
band-limited interpolant (S2/S5). Each crossing is found by bracketing the
target phase between samples and solving on the interpolant. The method and
its error are chosen in plan Task 4.2 against the 4 ps budget.

## Parameters

| Register row | Value |
|--------------|-------|
| `S7.edge_budget` | 4 ps (derived) |
| `S7.pit_polarity` | pit where cos φ > s (declared) |

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

Unit tests (plan Task 4.2): on a constant-frequency input, edge intervals
equal 1/(2f) within the 4 ps budget, and polarities alternate.

## Diagnostic tap

`recording_limiter`: D4 as binary records (`float64` time, `int8` polarity)
with a descriptor.

## Open items

None.
