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
  delay. A causal filter cannot have negative delay, so a constant is
  needed to make the response realisable.

The stage then advances its output by `τ_b` (an exact shift of the time
origin, possible because the model is not real time). So a 0.5 MHz component
leaves with zero delay, which is what "0.5 MHz reference" means. Signals are
never delayed by S3 as a whole.

### Design (plan Task 3.2)

**Target curve.** t_d(f) is the shape-preserving cubic of Fritsch and Butland
(SIAM J. Sci. Stat. Comput. 5, 1984) through (0, 0) and the table points,
with zero slope at both ends, held constant below 0 and above the last
point. It passes through every table point, has a continuous slope, and
between two points stays within their values. Below 0.5 MHz it is 0, the
middle of the ±15 ns band. Above 4.2 MHz it holds −200 ns.

**Filter.** An FIR filter of 48 taps spaced one 4fsc period (69.8 ns) apart,
applied at Fm as y[m] = Σₖ cₖ·x[m + D − 6k]. Its response repeats every 4fsc.
That is exact for D2, whose content S2 confines below 2fsc (images ≥ 91.5 dB
down), and it costs 48 multiply-adds per sample instead of the 288 of the
same span at full rate.

**Fit.** The taps minimise the weighted squared error between the filter's
complex response and the target e^{−j2π(f·τ_b + ∫₀ᶠ t_d)}. The grid runs from
0 to 6.8 MHz in 10 kHz steps. The weight is 1 up to the last table
frequency and 0.1 above it, which keeps the magnitude flat. The normal
equations are solved by Cholesky factorisation, with a ridge of 10⁻⁶ of the
diagonal, because the band left free near 2fsc makes them nearly singular.
The taps are then scaled to sum to 1, so the DC gain is 1 and constant
video levels (sync tip, blanking, white) pass unchanged. The design runs
when the stage is built, because `delay_ns` is a project key; its sines and
cosines come from basic arithmetic only (`src/dsp/`), so the taps are the
same on every host.

**Bulk delay.** τ_b puts the spread of target delays in the middle of the
filter's span, (47/2)·T − min(t_d)/2. It is rounded to whole Fm periods, D,
so that removing it is an exact shift. For the §9.1.7 table, D = 150
(1 746.03 ns).

**Realised response** at the §9.1.7 table, relative to 0.5 MHz:

| f (MHz) | Nominal (ns) | Realised (ns) | Middle third of tolerance |
|---------|--------------|---------------|---------------------------|
| 2 | −15 | −15.6 | ±5 |
| 3 | −45 | −46.3 | ±5 |
| 3.58 | −80 | −81.7 | ±5 |
| 4 | −135 | −141.1 | ±10 |
| 4.2 | −200 | −193.2 | ±16.7 |

The magnitude is within 0.0064 dB from 0 to 4.2 MHz and within 0.041 dB to
6.5 MHz. Above 6.8 MHz, outside the fit, it falls (−4.4 dB at 7 MHz,
−16 dB at 2fsc), where D2 has no content. The 4 MHz point is the furthest
from nominal because the target's slope goes to zero at 4.2 MHz.

**Interaction with pre-emphasis.** S4's transfer function has a large group
delay of its own: −200 ns at DC, −54 ns at 0.5 MHz and +8.4 ns at fsc (see
[S4](s04-video-pre-emphasis.md)).
The player's de-emphasis is the exact inverse of §9.2.4, so that delay
cancels between disc and player. §9.1.7 is therefore applied to the video
signal on its own, and S4's delay is not counted against it. The two stages
are linear and time-invariant, so their order does not change the result.
The order S3 → S4 follows the standard's structure: §9.1 is the video signal
and §9.2.4 is applied "prior to modulation".

**Above 4.2 MHz.** §9.1.7 is silent. The target holds the last delay, and
the filter keeps a flat magnitude to 6.5 MHz. This is recorded as a
modelling decision: NTSC video is band-limited to 4.2 MHz (SMPTE 170M), and
the player low-pass filter that §9.1.7 equalises removes content above it.

## Parameters

| Register row | Value |
|--------------|-------|
| `S3.enabled` | `true` |
| `S3.delay_points` | The §9.1.7 table |
| `S3.target_curve` | Monotone cubic through the table |
| `S3.filter_taps` | 48 at 4fsc spacing |
| `S3.design_band` | Weighted least squares, 0–6.8 MHz |
| `S3.bulk_delay` | Span centre, whole Fm periods |

## Assumptions and approximations

- The realised delay between table frequencies follows the filter's smooth
  curve, not Figure 8's straight lines. The requirement is that it stays
  inside the Figure 8 band, whose edges are straight lines between the
  tolerance points.
- The magnitude error of the realised filter is ≤ 0.1 dB to 4.2 MHz (plan
  Task 3.2); realised: 0.0064 dB.
- Outside the run the input is 0 IRE, as in S2.

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

`tests/unit/test_group_delay_predistorter.cpp`:

- group delay at the six table frequencies within the middle third of each
  tolerance (plan Task 3.2);
- group delay inside the Figure 8 band every 10 kHz from 0 to 4.2 MHz;
- magnitude within ±0.1 dB to 4.2 MHz, taps summing to 1;
- a streamed 3.58 MHz sinusoid has the designed filter's gain and phase,
  which checks the advance;
- `enabled: false` passes D2 unchanged; output identical for any block size.

**Independent check.** `scripts/verify_stages.py` (plan Task 6.2) checks the taps of real runs with tools written independently of the implementation ([scripts/README.md](../../../scripts/README.md)). The response is measured, not designed: H =
P_xy/P_xx (Welch, 2¹⁵-sample Hann segments) between the `video_rate_converter`
and `group_delay_predistorter` taps of one frame of a photograph with NTC-7
and FCC test lines (2T, multiburst, bars). Group delay is the slope of a
power-weighted line fit to arg H over ±50 kHz:

| f (MHz) | Nominal (ns) | Measured (ns) | Realised, above (ns) | Middle third |
|---------|--------------|---------------|----------------------|--------------|
| 2 | −15 | −15.5 | −15.6 | ±5 |
| 3 | −45 | −46.4 | −46.3 | ±5 |
| 3.58 | −80 | −81.0 | −81.7 | ±5 |
| 4 | −135 | −140.5 | −141.1 | ±10 |
| 4.2 | −200 | −192.2 | −193.2 | ±16.7 |

Largest magnitude error, every 100 kHz from 0.1 to 4.2 MHz: +0.0063 dB
(limit ±0.1 dB). The measured delays are within 0.7 ns of the designed
filter's, which is the resolution of the ±50 kHz fit.

## Diagnostic tap

`group_delay_predistorter`: D2 after the stage.

## Open items

- None for the specification. Whether a given decoder applies the matching
  playback equalisation is a measurement, made through the 12.5T pulse in the
  round trip ([metrics](../ntsc-round-trip-metrics.md)).
