# S3 — Group-delay pre-distortion (`group_delay_predistorter`)

## Physical process

Before cutting, the mastering chain pre-distorts the group delay of the video
signal. The purpose, in the standard's words, is "to equalize the group delay
distortion of the playback low-pass filter": a player's video low-pass filter
delays high frequencies more than low ones, and the disc carries the opposite
distortion so that the player's output is flat.

## Sources

- IEC 60857-1986 §9.1.7, the table and Figure 8.
- IEC 60857-1986 Amendment 2 replaces Figure 8. The new figure has the same
  curve and tolerance band; its axis label reads 3.58 MHz where the original
  reads 3.6.

## Model and equations

The §9.1.7 specification, with delay relative to 0.5 MHz:

| f (MHz) | t_d (ns) | Tolerance (ns) |
|---------|----------|----------------|
| 0.5 | 0 (reference) | — |
| 2 | −15 | ±15 |
| 3 | −45 | ±15 |
| 3.58 | −80 | ±15 |
| 4 | −135 | ±30 |
| 4.2 | −200 | ±50 |

Figure 8 also shows a ±15 ns band from 0 to 0.5 MHz, and draws the nominal
curve and the band edges as straight lines between the table frequencies.

The stage is a filter with:

- **flat magnitude** — §9.1.7 specifies delay only;
- **group delay** `τ(f) = τ_b + t_d(f)`, where `τ_b` is a constant bulk
  delay. A causal filter cannot have negative delay, and `τ_b` is the
  smallest constant that makes the response realisable.

The stage then advances its output by `τ_b` (an exact shift of the time
origin, possible because the model is not real time). So a 0.5 MHz component
leaves with zero delay, which is what "0.5 MHz reference" means. Signals are
never delayed by S3 as a whole.

The filter design method (for example, a cascade of all-pass sections fitted
to the table) is chosen in plan Task 3.2. Its target is the **nominal**
curve, so that the realised delay sits in the middle of each tolerance.

**Interaction with pre-emphasis.** S4's transfer function has a large group
delay of its own: +200 ns at DC, +54 ns at 0.5 MHz and −8.4 ns at fsc (see
[S4](s04-video-pre-emphasis.md)).
The player's de-emphasis is the exact inverse of §9.2.4, so that delay
cancels between disc and player. §9.1.7 is therefore applied to the video
signal on its own, and S4's delay is not counted against it. The two stages
are linear and time-invariant, so their order does not change the result.
The order S3 → S4 follows the standard's structure: §9.1 is the video signal
and §9.2.4 is applied "prior to modulation".

**Above 4.2 MHz.** §9.1.7 is silent. The filter keeps a flat magnitude there,
and its delay is whatever the design gives. This is recorded as a modelling
decision: NTSC video is band-limited to 4.2 MHz (SMPTE 170M), and the player
low-pass filter that §9.1.7 equalises removes content above it.

## Parameters

| Register row | Value |
|--------------|-------|
| `S3.enabled` | `true` |
| `S3.delay_points` | The §9.1.7 table |

## Assumptions and approximations

- The realised delay between table frequencies follows the filter's smooth
  curve, not Figure 8's straight lines. The requirement is that it stays
  inside the Figure 8 band, whose edges are straight lines between the
  tolerance points.
- The magnitude error of the realised filter is ≤ 0.1 dB to 4.2 MHz (plan
  Task 3.2).

## Input/output contract

- **In/out:** D2. The time origin is unchanged; the bulk delay is removed.

## Project YAML block

`group_delay_predistorter`:

| Key | Type | Default | Range | IEC tolerance |
|-----|------|---------|-------|---------------|
| `enabled` | bool | `true` | — | `false` is out of tolerance (§9.1.7 "shall") |
| `delay_ns` | list of `[f_mhz, ns]` | the §9.1.7 table | f in 0.5–4.2 MHz, ascending; the first point must be `[0.5, 0]` | each point within its §9.1.7 tolerance |

## Ideal setting

On, with the nominal §9.1.7 curve. "Ideal" means a disc nominal in every IEC
parameter, and §9.1.7 is mandatory. `enabled: false` exists only for A/B
comparisons and must be listed in `allow_out_of_tolerance`.

## Verification

Unit tests measure the group delay at the six table frequencies and require it
within the middle third of each tolerance (plan Task 3.2). Magnitude is within
±0.1 dB to 4.2 MHz.

## Diagnostic tap

`group_delay_predistorter`: D2 after the stage.

## Open items

- None for the specification. Whether a given decoder applies the matching
  playback equalisation is a measurement, made through the 12.5T pulse in the
  round trip ([metrics](../ntsc-round-trip-metrics.md)).
