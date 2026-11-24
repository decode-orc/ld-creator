# Sources to acquire

What the disc model still lacks, most important first. For each item: the
best figure available now, what is still missing, and how to get it. The
sources behind the figures are in
[secondary-sources.md](secondary-sources.md), mostly under
[Approximate figures](secondary-sources.md#approximate-figures-from-the-final-search).
When an item is resolved, its entry is removed.

**Nothing here blocks Phase 5.** The two questions that affected the ideal
disc are answered: the Gaussian pupil weighting applies once
([S11](s11-optical-readout.md#open-items)), and the DdD taps the
LD-V4300D before its RF EQ AMP ([S12](s12-player-front-end.md#sources)).

Most remaining gaps are best closed by measurement: DdD captures (plan
Task 10.2) for items 2, 7, 8 and 9, and a microscope image of a disc for
items 5 and 6.

## Phase 5 (checks, not blocking)

### 1. S11 transfer function

- **Now:** the integration matches the analytic uniform-pupil MTF (0.2507
  at 802 /mm). Toshiba's Hi-Vision videodisc paper (1990) gives a roll-off
  within about 1–2 dB of the model at the same normalised frequency.
- **Missing:** a published value for a Gaussian-filled pupil that separates
  "once" from "uniform" (0.213 vs 0.250).
- **Get:** Hopkins, *JOSA* 69(1) 4–24 (1979),
  [doi:10.1364/JOSA.69.000004](https://doi.org/10.1364/JOSA.69.000004)
  (paywalled); or Bouwhuis et al. 1985
  ([archive.org loan](https://archive.org/details/isbn_0852747853_v3i0));
  or Task 10.2.

### 2. LD-V4300D pickup OEIC (PA0037) response

- **Now:** −3 dB at roughly 15–25 MHz, inferred from pickup OEICs of the
  period (Sharp, 1997: 18–21 MHz). It must pass at least about 14 MHz for the
  player to work. Modelled as flat; a single pole at 15–20 MHz would droop a
  few tenths of a dB at 8–9 MHz.
- **Missing:** the actual response.
- **Get:** measure it (Task 10.2: compare the captured carrier level against
  the S11 prediction over radius). No datasheet has been found.

### 3. ADS825

- **Resolved** from the datasheet; recorded in
  [S13](s13-rf-capture.md#open-items). It gives no per-code transition data,
  so S13's declared quantiser stands, with the datasheet's DNL and INL as
  its error bounds.

## Phase 8 (deterministic disc impairments)

### 4. Resist exposure constants (S8)

- **Now:** recorder line argon 457.9 nm (Sony US 4,152,726; Philips
  LHH0400). AZ 1350J Dill constants at 436 nm (Kim 1984: A = 0.55 µm⁻¹,
  B = 0.045 µm⁻¹, C = 0.013 cm²/mJ, with a development-rate fit) and at
  404.7 nm (Dill & Shaw 1977). Development nonlinearity γ = 2–2.5 for
  AZ1350B at 442 nm (RCA 1978).
- **Missing:** constants at 457.9 nm.
- **Get:** Jacobs, *Appl. Opt.* 17(13) 2001 (1978),
  [doi:10.1364/AO.17.002001](https://doi.org/10.1364/AO.17.002001)
  (paywalled). Otherwise use the 436 nm set and declare it.

### 5. Pit width and wall angle (S9)

- **Now:** depth 0.12 µm. Wall angle 45–50° from the normal (Philips design
  value, acceptable range 30–65°; US 4,230,915). Width 0.4–0.6 µm,
  decreasing with radius (the patent's example: 800 nm inner, 500 nm
  outer). With 45° walls, the land-plane and half-depth widths differ by
  0.12 µm, which may account for the 0.4 vs 0.6 µm disagreement.
- **Missing:** a measurement of a pressed LaserVision disc.
- **Get:** AFM or SEM of a disc. Otherwise Legierse & Pasman (1989,
  [Springer](https://rd.springer.com/chapter/10.1007/978-1-4613-0843-0_11)).

### 6. Replication fidelity (S9)

- **Now:** moulding (NTSC) transfers 72–96 % of the stamper depth (CD data,
  Hougum 1996), poorer at the outer radius. 2p (European discs) shrinks by
  at most about 15 % by volume (PTR 1982), so at most about 6–18 nm of a
  0.12 µm pit.
- **Missing:** LaserVision measurements; any figure for electroforming.
- **Get:** as item 5.

## Phase 9 (noise, crosstalk, timing)

### 7. Crosstalk

- **Now:** about −40 to −50 dB relative to the on-track signal at 1.67 µm,
  worsening by about 4 dB per 0.1 µm off-track (Kondo 1993; Toshiba 1990;
  both scaled from other systems).
- **Missing:** LaserVision values against off-track error, for CAV and CLV.
- **Get:** Velzel, *Appl. Opt.* 17(13) 2029 (1978),
  [doi:10.1364/AO.17.002029](https://doi.org/10.1364/AO.17.002029)
  (paywalled); or measure it from captures.

### 8. Noise spectrum

- **Now:** carrier-to-noise 58–63 dB, taken as in 30 kHz (the IEC 60857
  spectrum convention). Disc noise falls with frequency, following the
  readout MTF (BBC, Isailović). Diode-laser RIN below −132 dB/Hz (Hitachi
  1986), well under disc noise.
- **Missing:** measured density against frequency and radius.
- **Get:** Heemskerk, *Appl. Opt.* 17(13) 2007 (1978),
  [doi:10.1364/AO.17.002007](https://doi.org/10.1364/AO.17.002007)
  (paywalled); or measure it from a capture's noise floor at several radii.

### 9. Timing error above 30 Hz (S10)

- **Now:** uncorrected about 20 µs per revolution, falling about
  −12 dB/oct above the rotation rate to about 1 kHz. The tangential servo
  gives 70 dB at 30 Hz and about 1.6 kHz bandwidth (Isailović Fig. 3-47).
  Residuals: about 1 µs after the mirror, 80 ns from sync and 5 ns from burst
  after a TBC (Pioneer; Olympus). A 60 Hz line from the vertical interval.
- **Missing:** magnitudes at harmonics of 30 Hz.
- **Get:** measure them from captures (phase of the burst against line
  number).

## Other

### 10. PWM spectrum for a sinusoidal carrier (S6, audio)

- **Now:** the triangle-carrier form (Sun 2012).
- **Missing:** the LaserVision case, a limited sine whose zero crossings
  the audio carriers shift. To be derived; no source needed.

### 11. DE0-Nano oscillator (S13)

- **Now:** ±50 ppm, typical of 3.3 V 50 MHz SMD oscillators (range ±20 to
  ±100 ppm). Terasic's schematic marks Y1 only "50MHZ". The DdD clock is
  exactly 50 MHz × 8/5 ÷ 2, so the error passes straight to the sample rate.
- **Missing:** the part.
- **Get:** read the marking on a board, or measure CLOCK_50.
