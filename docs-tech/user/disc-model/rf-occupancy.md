# RF occupancy by test pattern

Results of plan Task 6.3: for each test pattern, the instantaneous-frequency
range, the RF's occupied bandwidth and the residual above the capture
Nyquist frequency. Measured on the ideal disc by
[`scripts/verify_stages.py`](../../../scripts/README.md), without ld-decode.

## Method

- **Input:** one frame of each bundled legal-range videosynth pattern
  ([`scripts/projects/patterns_videosynth.yaml`](../../../scripts/projects/patterns_videosynth.yaml)),
  with no test lines and no impairments. 100 % bars and the full-range ramp
  are left out: their levels are outside the legal range, so no programme
  would carry them. Each pattern is run as a one-frame
  project at CAV 55 mm and 144.99 mm with the `domesday_duplicator` capture
  profile, and at 144.99 mm with `ideal`. The pitch is 1.6667 µm, and every
  other key is at its ideal default.
- **Instantaneous frequency:** from the `fm_phase_modulator` tap, the phase
  advance over each Fm interval (11.6 ns) divided by 2π·(1/Fm). This is the
  mean over the interval, so a peak narrower than 11.6 ns would read low.
  The column "H(f max)" is the readout transfer at the pattern's peak
  carrier, relative to its value at the 8.1 MHz blanking carrier, at
  55 mm (S11 note prediction).
- **Occupied band:** the band holding 99 % of the AC power of the player
  RF (`player_front_end` tap, 80 MHz), with 0.5 % cut from each side (Welch,
  2¹⁴-sample Hann segments).
- **Residual above Nyquist:** the power of the capture filter's output
  (`rf_capture` tap, 80 MHz, before the converter takes every second
  sample) above 20 MHz, relative to its total. This is the power that the
  decimation aliases. "Rejection" is the player RF's fraction above 20 MHz
  divided by the residual: the filter's effective stop-band attenuation
  on that spectrum.

## Results

Frequencies are in MHz. The instantaneous-frequency columns do not depend on
the radius.

| Pattern | f min | f max | H(f max), 55 mm | Occupied, 55 mm | Occupied, 144.99 mm | Player RF > 20 MHz, 144.99 mm | DdD residual, 144.99 mm | DdD rejection | Ideal residual, 144.99 mm | Ideal rejection |
|---------|-------|-------|-----------------|-----------------|---------------------|-------------------------------|-------------------------|---------------|---------------------------|-----------------|
| BLACK | 7.210 | 8.728 | −2.2 dB | 7.35–8.23 | 7.49–8.39 | −24.2 dB | −77.5 dB | 53.3 dB | −137.6 dB | 113.4 dB |
| 75_BARS | 7.079 | 10.275 | −9.3 dB | 4.61–9.38 | 4.87–12.39 | −24.8 dB | −79.5 dB | 54.7 dB | −134.6 dB | 109.8 dB |
| SMPTE_BARS_001 | 7.075 | 10.214 | −9.0 dB | 4.58–9.32 | 4.88–12.38 | −24.8 dB | −79.4 dB | 54.6 dB | −134.6 dB | 109.8 dB |
| PLUGE | 7.210 | 9.918 | −7.4 dB | 7.32–9.32 | 7.49–9.42 | −24.4 dB | −77.8 dB | 53.4 dB | −137.1 dB | 112.7 dB |
| GREY_5H_STEP | 7.210 | 10.275 | −9.3 dB | 7.20–9.37 | 7.49–9.60 | −25.6 dB | −79.7 dB | 54.1 dB | −137.0 dB | 111.4 dB |
| LUMA_RAMP | 7.210 | 9.936 | −7.5 dB | 6.99–9.29 | 7.47–9.42 | −25.6 dB | −79.9 dB | 54.2 dB | −136.3 dB | 110.6 dB |
| LEGAL_RAMP | 6.126 | 11.548 | −18.6 dB | 3.51–9.29 | 4.70–12.81 | −24.9 dB | −79.8 dB | 54.9 dB | −133.8 dB | 108.9 dB |
| MULTIBURST | 7.132 | 10.431 | −10.2 dB | 4.05–9.32 | 5.18–12.33 | −25.3 dB | −79.8 dB | 54.5 dB | −135.1 dB | 109.8 dB |
| LUMA_ZONE_PLATE | 7.119 | 10.386 | −10.0 dB | 3.83–9.39 | 5.30–12.14 | −25.3 dB | −79.8 dB | 54.5 dB | −135.0 dB | 109.7 dB |
| CHROMA_ZONE_PLATE | 7.210 | 9.470 | −5.3 dB | 6.07–8.83 | 7.48–8.99 | −25.8 dB | −80.3 dB | 54.4 dB | −136.2 dB | 110.4 dB |
| GREEN_MAGENTA_EDGES | 6.709 | 11.034 | −14.2 dB | 3.92–9.01 | 4.97–12.45 | −25.0 dB | −79.7 dB | 54.7 dB | −134.2 dB | 109.2 dB |
| TARTAN | 6.915 | 10.311 | −9.5 dB | 4.61–9.37 | 4.86–12.39 | −24.8 dB | −79.5 dB | 54.6 dB | −134.5 dB | 109.7 dB |
| PHOTO_BUTTERFLY | 7.210 | 9.930 | −7.5 dB | 5.16–9.16 | 7.46–9.40 | −25.5 dB | −79.7 dB | 54.2 dB | −136.1 dB | 110.6 dB |

At 55 mm the readout cuts off at 13.09 MHz, so the player RF has no power
above 20 MHz. The DdD filter's output there is −189 to −192 dB of its total:
rounding error.

## Against the stage notes

- **S4 overshoot limits.** Every pattern stays inside the 4.78–12.15 MHz
  that [S4](s04-video-pre-emphasis.md) derives for an instantaneous
  sync-to-white step. The widest swing is LEGAL_RAMP's, 6.13–11.55 MHz. A
  4fsc input is band-limited, so its overshoot is smaller than an ideal
  step's, as the note expects.
- **S11 attenuation of overshoots.** [S11](s11-optical-readout.md)
  predicted that excursions towards 12 MHz are strongly attenuated by the
  readout at the inner radius. Measured: patterns peak at 8.7–11.5 MHz,
  where H at 55 mm is 2–19 dB below its value at the blanking carrier.
- **S13 stop band.** [S13](s13-rf-capture.md) gives the nominal DdD chain at
  least 47.3 dB down above 20 MHz, and the `ideal` profile at least 98 dB.
  On these spectra the effective rejection is 53.3–54.9 dB and
  108.9–113.4 dB. The highest residual, −77.5 dB of the RF power, is at
  144.99 mm on a black field, where the third harmonic of the pit train
  (about 24 MHz) is strongest.
- **Occupied band.** At 144.99 mm the RF reaches 12.8 MHz (LEGAL_RAMP)
  before the 0.5 % tail. At 55 mm the readout attenuates the upper
  sidebands, so the occupied band shifts down, to as low as 3.5 MHz on
  the legal ramp.

Nothing here contradicts a stage note, so Task 6.3 raises no open item.
