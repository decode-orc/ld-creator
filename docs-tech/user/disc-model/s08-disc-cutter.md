# S8 — Disc cutter (`disc_cutter`)

## Physical process

The laser beam recorder writes the limited signal into photoresist on a
rotating glass master. The focused laser spot moves outward along a spiral
while the disc turns, so recording time becomes position on the disc. Where
the laser is on, the exposed resist is later developed away and leaves a pit.

## Sources

- IEC 60857-1986 §4.7.1: CAV, 1 rotation per TV frame.
- §4.7.2: CLV, linear velocity 10.1–11.4 m/s; §4.7.3 and Figure 19:
  angular acceleration no more negative than −0.32 rad/s².
- §4.6: anti-clockwise rotation seen from the read side.
- §4.10–4.12: lead-in starts at ≤ 53.5 mm; the programme starts at ≥ 55 mm
  and ends at ≤ 145 mm (12-inch).
- §4.15: track pitch 1.4–2.0 µm.
- Figure 18 note: pit spatial frequency `V = f / (2πR·fr)` pits/mm.
- BBC RD 1986/01 §2.4: the master is photoresist on glass, exposed by a blue
  laser; §3.6: the effective cutting spot "appears to be about 0.6 µm".
- Philips Technical Review 33 (1973) p. 179: real-time laser cutting into a
  prepared glass disc. B.A.J. Jacobs, "Laser beam recording of video master
  disks", Applied Optics 17(13) 2001–2006 (1978): positive photoresist, an
  electro-optic intensity modulator ([secondary sources](secondary-sources.md)).
- Isailović, *Videodisc and Optical Memory Systems* (1985) ([Isailović 1985](secondary-sources.md#isailović-videodisc-and-optical-memory-systems-1985)):
  §2.3.1.4, the cut edges mark the zero crossings of the limited signal;
  App. 2.2, the exposure model; §2.3.1.2, edge distortion of about 0.1 µm;
  §2.3.1.6, the 40–60 % duty-cycle acceptance limit.

## Model and equations

The cutter maps each D4 edge time `t_k` to a position on the spiral: arc
length `x` along the track and radius `r`.

**CAV.** The angular velocity is one revolution per frame,
`ω = 2π·fr` with `fr` = 30000/1001 Hz. The radius advances by one track
pitch `p` per revolution:

```
r(t) = r₀ + p·fr·t
x(t) = ∫₀ᵗ ω·r(u) du = 2π·fr·(r₀·t + p·fr·t²/2)
```

**CLV.** The linear velocity `v` is constant. Each revolution covers an
annulus of width `p`, so the area swept per unit time is `p·v`:

```
2π·r·dr = p·v·dt   ⇒   r(t) = √(r₀² + p·v·t/π),   x(t) = v·t
```

The CLV angular acceleration follows from ω = v/r:
`dω/dt = −p·v² / (2π·r³)`. At the worst case allowed by §4.7.2 and §4.15
(v = 11.4 m/s, p = 2.0 µm, r = 53.5 mm), this is −0.27 rad/s², inside the
§4.7.3 limit, so a pure CLV spiral always satisfies §4.7.3. Figure 19 also
permits stepped CLV (constant-*n* stretches) to reduce crosstalk; that is
not modelled.

**Spatial frequency check.** Figure 18's note gives the pit spatial frequency
`V = f / (2πR·fr)` pits/mm for CAV. The HFMI is specified at
V = 802 ± 26 pits/mm. At the 8.1 MHz blanking carrier this is the radius
band 51.9–55.4 mm, so the HFMI reference is the CAV blanking carrier at the
inner edge of the programme area.

**Edge profile.** The cut edge is the exposure threshold contour of the
cutting spot convolved with the laser's on/off signal. At the ideal setting
each D4 edge maps to a perfect step at its position, with no length bias.
The exposure model (spot profile, resist threshold, and the resulting pit
length bias and edge slope) is Phase 8 work (plan Task 8.2). Its starting
point is the 0.6 µm effective spot of BBC §3.6.

The form of that model is sourced (Isailović App. 2.2). The exposure along
and across the track is

```
E(u, v) = (1/s) ∫ I(u′) · h(u − u′, v) du′
```

with `I` the laser intensity, `h` the cutting-spot intensity profile and `s`
the track speed. Two development thresholds bound the pit: resist starts to
etch at E₂ and is cleared to full depth at E₁. The E₂ contour gives the
pit's outer length and width, the E₁ contour its floor, and the gap between
them the wall slope. In CAV, `E ∝ 1/s ∝ 1/R`, which is why the recorder
raises the laser power with radius (§2.3.1.2, §2.6.1). Task 8.2 still needs
values for `h`, E₁ and E₂.

**Bound on the length bias.** A pit length bias `b` moves the duty cycle of
a carrier of frequency `f` at track speed `v` by `b·f/v`. Isailović §2.3.1.6
accepts a playback duty cycle of 40–60 %, which bounds `|b| ≤ 0.1·v/f`. That
is 0.13 µm for the 8.1 MHz blanking carrier at 55 mm (v = 10.36 m/s), the
same order as the "approximately 0.1 µm" of edge distortion in §2.3.1.2.
This is an acceptance limit on the whole recorded chain, not a value for the
cutter alone.

## Parameters

| Register row | Value |
|--------------|-------|
| `S8.format` | `cav` or `clv` (required) |
| `S8.start_radius` | required |
| `S8.track_pitch` | required; 1.4–2.0 µm |
| `S8.linear_velocity` | required for CLV; 10.1–11.4 m/s |
| `S8.frame_rate` | 30000/1001 Hz (fixed) |

**Why there are no defaults.** The standard gives ranges, not nominal values,
for the format, radius, pitch and CLV velocity. Choosing a default would be an
undocumented choice, so the block is required and each project states its
values. The example projects use a track pitch of 1.6667 µm, derived as the
programme band (145 − 55 = 90 mm) divided by the 54 000 frames of a one-sided
NTSC CAV disc given in the CVBS specification's file-size table
([index.md](../../cvbs-file-format-specification/docs/index.md)). This lies
inside §4.15.

## Assumptions and approximations

- The cutter's spiral is perfect: constant pitch, no radial or tangential
  error. Mastering errors are deterministic impairments for Phase 8.
- The disc's turntable speed during cutting is exact (§4.7.1/§4.7.2 at the
  configured value).

## Input/output contract

- **In:** D4 (edge times).
- **Out:** D5 — for each edge, `x_k = x(t_k)`, `r_k = r(t_k)` and polarity,
  plus the trajectory functions `x(t)`, `r(t)` for S10.

## Project YAML block

`disc_cutter` (required):

| Key | Type | Required | Range | IEC tolerance |
|-----|------|----------|-------|---------------|
| `format` | string | Yes | `cav`, `clv` | — |
| `start_radius_mm` | float | Yes | 41.5–150 (clamping area outer radius §4.5.2 to disc radius §4.2) | the run must end at ≤ 145 mm (§4.12) |
| `track_pitch_um` | float | Yes | 0.5–5.0 | 1.4–2.0 (§4.15) |
| `linear_velocity_m_s` | float | CLV only | 5–20 | 10.1–11.4 (§4.7.2) |

`linear_velocity_m_s` on a CAV project is a validation error.

## Ideal setting

Perfect spiral and perfect edges at the configured format, radius and pitch.

## Verification

Unit tests (plan Task 4.3): radius after N revolutions equals `r₀ + N·p`; CLV
arc length equals `v·t`; CAV pit spatial frequency matches the Figure 18
formula.

## Diagnostic tap

`disc_cutter`: D5 as binary records (`float64` arc length, `float64` radius,
`int8` polarity) with a descriptor.

## Open items

- Values for the cutting-spot profile `h`, the thresholds E₁ and E₂, and
  hence the pit length bias, width and edge slope (Phase 8). The model form
  and a bound on the bias are now sourced (above); the values need resist and
  recorder data (Jacobs 1978, Pasman et al. 1985;
  [secondary sources](secondary-sources.md)).
