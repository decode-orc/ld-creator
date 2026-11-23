# S12 — Player front end (`player_front_end`)

## Physical process

The photodiode current is amplified by the player's preamplifier and brought
out as the RF signal (on a player modified for capture, at its RF test
point). A real preamplifier has a finite bandwidth and adds noise. Players
also boost the high frequencies that the readout spot attenuates (aperture
or MTF compensation), because otherwise the FM sidebands come out unequal
and the demodulated video depends on radius.

## Sources

- IEC 60857-1986 §11.2: the photodiode and amplifier "shall have an adequate
  bandwidth".
- BBC RD 1986/01 §3.2.2 and Fig. 8: measured response of a domestic player's
  photodiodes and preamplifier (VLP830). §3.3 and Fig. 11: the preamplifier's
  first stage, with the four focus quadrants commoned through capacitors for
  the data signal.
- DomesdayDuplicator hardware guide (Filter stage): "LaserDisc players often
  employ a filter which has a stop-band of 13.2 MHz".
- Isailović 1985 §3.4, App. 3.1 and App. 3.2
  ([Isailović 1985](secondary-sources.md#isailović-videodisc-and-optical-memory-systems-1985)):
  players compensate the readout MTF with a cosine equaliser `1 − 2a·cos ωτ`
  (example τ ≈ 35 ns, peak boost at 14.4 MHz, a < 0.5), in some designs
  varied with radius from the burst amplitude. Compensation boosts "the
  high-frequency components of a (recorded) signal without affecting their
  phase". Without it, chroma demodulates larger relative to luma at the
  inner radius. §3.1 computes 13.2 MHz as the optical cut-off at the 55 mm
  inner radius, the same figure as the DdD guide's stop-band.
- ld-decode's command line (interface only): `-m/--MTF`, "mtf compensation
  multiplier", and `--MTF_offset`. Its help states that NTSC uses a readout
  MTF model. So by default ld-decode expects RF that has **not** been
  equalised, as at a player's test point (see "Decoder settings" below).

## Model and equations

S12 is a preamplifier followed by an optional MTF equaliser. **The ideal
setting has no equaliser.** ld-creator's output stands in for a real
capture, and a capture is taken at the player's RF test point, where the
readout MTF is still in the signal. ld-decode is built for that signal and
compensates the MTF itself (`-m/--MTF`). Equalising here would remove real
physics and test ld-decode with its compensation switched off.
The equaliser exists only as a **diagnostic**: comparing an `inverse` run
decoded with `-m 0` against a normal run decoded with ld-decode's defaults
tells you whether an inner-radius failure comes from S11 or from the
decoder's MTF compensation.

### Preamplifier

At the ideal setting, §11.2's "adequate bandwidth" is taken literally: flat,
unity gain, noiseless. Any other gain is absorbed by the capture level
setting in S13, so unity costs nothing.

### No equaliser (`mtf_equaliser: none`, the ideal)

`D9 = D8`: the RF at an unequalised test point.

### MTF equaliser (`mtf_equaliser: inverse`, diagnostic)

The diagnostic equaliser is the exact inverse of the S11 transfer function over
the band the optics pass, tracking the scanning speed:

```
G(f, τ) = 1 / H(f / v(τ))   for |f| < fc·v(τ)
G(f, τ) = 0                 for |f| ≥ fc·v(τ)
```

`H` and `fc = 2NA/λ` are from [S11](s11-optical-readout.md), and `v(τ)` is
the track speed from S8/S10. `H` is real and non-negative (the line-spread
function is symmetric), so `G` is real too and adds no phase, as Isailović
§3.4 asks of a compensator. `G(0) = 1/H(0) = 1`, so unpitted land still reads 1.

**Why this form.** For a diagnostic it must remove the S11 MTF and nothing
else. It is the only equaliser with no free parameter. It
recovers everything the optics passed, so the RF that reaches S13 is flat up
to the optical cut-off. The cut-off itself is unchanged, because nothing can
restore content the spot did not pass. A cosine equaliser would need `a` and
`τ`, and the book gives only example values for those. Tracking `v(τ)`
exactly is the ideal limit of the radius-dependent compensator in
Isailović App. 3.2.

**Result in the time domain.** The cascade S11 → S12 is an ideal spatial
low-pass at `fc`. Its line-spread function is `2fc·sinc(2fc·x)` (normalised
sinc) and its edge response is

```
E(x) = 1/2 + Si(2π·fc·x) / π
```

so, with the S11 notation,

```
D9:  i_eq(τ) = 1 − m · Σ_k σ_k · E(x_s(τ) − x_k)
```

`E` is the S11 edge response with `ESF` replaced by the band-limited step.
Unlike `ESF`, it overshoots: the Gibbs overshoot is about 9 % of each step,
so `i_eq` leaves the range [1 − m, 1]. That is correct for an electrical
signal after the preamplifier. S13 sets its level from this signal (its
blanking-carrier reference is taken after S12).

**Implementation.** Dividing D8 by `H` is ill-conditioned near the cut-off,
where `H → 0`. Because S11's linear model is a sum of edge responses, S12
computes `i_eq` from the D7 edges and trajectory with `E` instead. This is
the same signal, computed exactly. The shortcut holds only for the linear
readout model; the phase-pit model (plan Task 10.1) will need `G` applied as
a filter, with a stated gain limit.

**Sampling.** `i_eq` is band-limited to `fc·v ≤ 35.7 MHz`, as D8 is, so the
80 MHz readout rate still introduces no aliasing.

### Decoder settings

Nothing may be compensated twice, and nothing left uncompensated:

| RF | ld-decode |
|----|-----------|
| `none` (the ideal disc) | defaults: ld-decode compensates the MTF |
| `inverse` (diagnostic) | `-m 0` |
| `carrier_diagnostic` mode (no S11) | `-m 0` |

Whether `-m 0` on its own gives a flat channel is checked by the
carrier-diagnostic baseline ([metrics](../ntsc-round-trip-metrics.md)).

## Parameters

| Register row | Value |
|--------------|-------|
| `S12.profile` | `iec_reference` (flat, unity, noiseless preamplifier) |
| `S12.mtf_equaliser` | `none` (declared: test-point RF); `inverse` is diagnostic |

## Assumptions and approximations

- Flat, noiseless preamplifier. Error against a real player: player-specific;
  this is what plan Task 9.3 adds.
- Test-point RF is unequalised. This is the input ld-decode's own MTF
  compensation implies. Whether a particular player's test point follows its
  equaliser is a per-player open item (below).
- The diagnostic equaliser tracks the scanning speed exactly and has unlimited gain
  below the cut-off. This is exact for a noiseless disc. With noise
  (Phase 9), unlimited gain near the cut-off amplifies noise without bound,
  so Phase 9 must give the equaliser a gain limit with a stated reason.
- Real players use a fixed or burst-controlled cosine equaliser, which is
  not modelled here (plan Task 9.3).

## Input/output contract

- **In:** D8 at Fr, and the D7 edges and trajectory (used by the `inverse`
  equaliser). **Out:** D9 at Fr, normalised so land = 1.

## Project YAML block

`player_front_end`:

| Key | Type | Default | Values |
|-----|------|---------|--------|
| `profile` | string | `iec_reference` | `iec_reference` |
| `mtf_equaliser` | string | `none` | `none`, `inverse` (diagnostic) |

Further profiles (for example `bbc_vlp830`, from BBC Fig. 8) are added with
their sources in plan Task 9.3.

## Ideal setting

`iec_reference` preamplifier, no equaliser: `D9 = D8`, the test-point RF.

## Verification

- `none`: identity unit test.
- `inverse`: for a constant-frequency pit train, the D9 fundamental equals
  the pit pattern's fundamental, `(4/π)·m/2` peak, at every radius where it
  lies below the cut-off. The spectrum of D9 divided by that of D8 equals
  `1/H` within 10⁻⁶ wherever `H > 0.05`. The edge response reproduces the
  `Si` form above.

## Diagnostic tap

`player_front_end`: D9 as `float64` at Fr with a descriptor (written only
when `mtf_equaliser` is `inverse`; with `none` it is identical to the S11
tap).

## Open items

- Digitising BBC Fig. 8 into a documented response, with the digitising
  method and its reading error (plan Task 9.3).
- Whether the RF test point of a typical player follows the player's RF
  low-pass filter (the 13.2 MHz stop-band noted in the DomesdayDuplicator
  guide) and its MTF equaliser (Isailović §3.4). Player-specific; needs a
  service manual source per player.
- Gain limit for the diagnostic equaliser once noise exists (Phase 9).
