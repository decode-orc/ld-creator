# S10 — Playback mechanics (`playback_mechanics`)

## Physical process

The player spins the disc and its servos keep the readout spot focused and on
the track. The spindle is phase-locked to the syncs read from the disc, so
the playback rate tracks the recording. Mechanical imperfections of the disc
(eccentricity, warp) and of the player move the spot relative to the pits.

## Sources

- IEC 60857-1986 §4.7: rotation speeds (as S8).
- §4.16: vertical deviation (up to +0.9/−1.2 mm static; 100 m/s² below
  1.1 kHz; ±2 µm above 1.1 kHz). Amendment 2 §4.16.4: radial tilt ±1°.
- §4.18: radial deviation 160 µm during one revolution; ±0.1 µm above
  2.2 kHz; 20 m/s² below 2.2 kHz.
- §4.19: time base error at 55 mm 10 µs p-p (30 Hz roll-off, 12 dB/oct); at
  145 mm 4 µs p-p (CAV, 30 Hz) or 10 µs p-p (CLV, 10 Hz); shift between
  adjacent CAV tracks ±25 ns.
- §12.1.3 and Amendment 1 §12.1.4.5: residual radial tracking error.
- BBC RD 1986/01 §3.2.1: the player phase-locks the disc motor to replayed
  syncs against a quartz reference, and uses a tangential mirror for fine
  time-base correction.

## Model and equations

S10 gives the spot's position on the track as a function of playback time
`τ`: arc length `x_s(τ)` and radial offset `ρ(τ)` from the track centre.

At the ideal setting the player is locked to the disc and tracks perfectly:

```
x_s(τ) = x(τ)      (the S8 cutting trajectory, evaluated at τ)
ρ(τ) = 0
```

So playback time equals recording time, and the photocurrent at `τ` is the
pit pattern that was cut at `t = τ`.

This is exact for an ideal disc read by a locked player. §4.19 bounds the
time-base error of a real disc, and §4.18 its eccentricity; both are
deterministic impairments for Phase 8 (plan Task 8.1), and §4.19 gives the
limits directly in time.

## Parameters

| Register row | Value |
|--------------|-------|
| `S10.radial_deviation_limit` | 160 µm per revolution (§4.18.1) |
| `S10.time_base_error_limit` | §4.19.1 table |
| `S10.adjacent_track_shift_limit` | ±25 ns (§4.19.2, CAV) |

These are limits for later impairments, not settings of the ideal stage.

## Assumptions and approximations

- Perfect lock and perfect tracking. The player's servo dynamics are not
  modelled (HLD §1.1 non-goal); only their residual errors will be, within
  the §4 and §12 limits.

## Input/output contract

- **In:** D6. **Out:** D7 — D6 plus the spot trajectory `x_s(τ)`, `ρ(τ)`.

## Project YAML block

None in the ideal scope. The block name `playback_mechanics` is reserved for
Phase 8 keys.

## Ideal setting

`x_s(τ) = x(τ)`, `ρ = 0`.

## Verification

Identity unit test: the trajectory equals the S8 trajectory.

## Diagnostic tap

None at the ideal setting.

## Open items

- None for the ideal disc. Impairment shapes within the §4.18 and §4.19
  limits are Phase 8 work. Isailović 1985 §3.5 and App. 3.3
  ([Isailović 1985](secondary-sources.md#isailović-videodisc-and-optical-memory-systems-1985)) give the dominant shape: eccentricity
  `R(t) = R + ΔR·cos ω_d t` at the rotation rate, with timing error
  `ΔR/(ωR)` (10 µs for 100 µm at 55 mm, matching §4.19) and higher-frequency
  error falling about 12 dB per octave (also §4.19). Shapes above the
  rotation rate are not given.
