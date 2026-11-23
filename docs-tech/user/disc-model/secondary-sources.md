# Secondary sources

Sources beyond IEC 60857 and the BBC report, what each establishes for each
stage, and what is still unsourced (plan Task 1.4). "Read" means the content
was examined directly. "Not read" means only the citation or abstract was
seen, and **nothing from that source is used in the model** until it is read.

## Read

### DomesdayDuplicator repository

github.com/simoninns/DomesdayDuplicator, commit `0f015fa0c5d7`. It is owned
by the project's maintainer and may be used freely.

| Stage | Location | What it establishes |
|-------|----------|---------------------|
| S13 | `hardware/pcb/RF Amplifier.kicad_sch` (rev 3.0.2) | Every front-end component value: termination, AC coupling, OPA690 gain network, the elliptic low-pass (two inductors, five capacitors, 47 Ω terminations) |
| S13 | `hardware/pcb/ADC.kicad_sch` | ADS825 10-bit ADC and its reference divider |
| S13 | `hardware/doc/DdD Gain and filter calculations.xlsx` | Measured amplitude response 10 kHz–21.5 MHz (sheet 1); predicted filter phase 0–16 MHz (sheet 2); gain table and clip level for all 15 switch settings (sheet 3) |
| S13 | `docs/content/development/hardware-guide.md` | Input impedance, gain theory, filter design (elliptic, −3 dB ≈ 13.8 MHz, < −35 dB at 20 MHz measured), 2 V p-p ADC window, working level 75 % of full scale, 40 MHz clock from an 80 MHz PLL |
| S13 | `docs/content/development/fpga-decimation-filter.md` | Rationale that frequency-dependent group delay disperses FM; the 20 MSPS decimator (not used for LaserDisc) |
| S14 | `docs/content/capture-gui/capture-files.md`; `ddd-gui/tests/unit/test_sample_format.cpp` | Capture sample format: signed 16-bit LE mono; 10-bit code *c* stored as (c − 512) × 64 |
| S12 | hardware guide, "Filter stage" | Players "often employ a filter which has a stop-band of 13.2 MHz" (not used in the reference model; recorded for the player profiles) |

**Proposed upstream:** none. The repository is maintained separately and is
the right home for this material.

### Philips Technical Review vol. 33 (1973), no. 7 — the VLP issue

A free scan of the 1973 volume is hosted at worldradiohistory.com
(`Philips-Technical-Review-1973.pdf`). The papers were reprinted in J. SMPTE
83(7), 1974.

- (a) K. Compaan & P. Kramer, "The Philips 'VLP' system", pp. 178–180.
- (b) W. van den Bussche, A.H. Hoogendijk & J.H. Wessels, "Signal processing
  in the Philips 'VLP' system", pp. 181–185.
- (c) G. Bouwhuis & P. Burgstede, "The optical scanning system of the Philips
  'VLP' record player", pp. 186–189.
- (d) P.J.M. Janssen & P.E. Day, "Control mechanisms in the Philips 'VLP'
  record player", pp. 190–193.

**Caution.** These describe the 1973 VLP prototype, not the LaserVision
format standardised in IEC 60856/60857. The prototype had a 4.75 MHz carrier,
chroma moved down to 1 MHz and sound on 250 kHz. Use these papers for
**physical mechanisms and orders of magnitude**, and never for LaserVision
parameter values, which come from IEC 60857.

| Stage | Location | What it establishes |
|-------|----------|---------------------|
| S6, S7 | (b) pp. 181–182, Fig. 3; (a) p. 181 | Signals summed then **symmetrically limited**; colour and sound carried as "symmetrical displacements of the zeros" (duty-cycle modulation); pits occupy on average half the track |
| S7, S8 | (a) p. 179 | Real-time laser cutting into a prepared glass disc; a "highly non-linear" photographic process, so the recording is two-level |
| S9 | (a) pp. 178–179 | Moulds made by electroplating; pressing roughness ≤ 0.01 µm; evaporated metal reflective layer |
| S9, S11 | (a) p. 178; (c) pp. 186, 189 | Prototype pits 0.8 µm wide and 0.16 µm deep (≈ λ/4); NA 0.4 "requires a width of 0.8 µm"; residual intensity rises with the square of the depth error |
| S11 | (c) pp. 186, 188 | NA 0.4, λ 0.63 µm, half-intensity spot ≈ 0.9 µm; Gaussian pupil filling using ≈ 80 % of the laser power. **Cross-checks** the IEC 23 % rim intensity (1 − 0.23 = 0.77) and the model's 0.87 µm spot |
| S11 | (c) pp. 186–187, Figs 1, 3, 5 | Maximum modulation when pit-bottom light is 180° out of phase with, and equal in intensity to, land light; modulation against period for focus errors of 0, 2.5 and 6 µm (Phase 8 focus) |
| S11 (Phase 9) | (a) p. 180; (c) p. 187, Fig. 4 | Crosstalk < −30 dB; −50 dB measured for 1 µm tracks; off-track errors up to ≈ 0.2 µm give no noticeable crosstalk |
| S11, S12 (Phase 9) | (a) p. 178; (c) p. 187 | Design aims for a thermal-noise-limited detector; low HeNe laser noise at MHz frequencies (no numbers) |

**Proposed upstream:** a transcription of the four VLP papers for
`analogue-video-specifications` (under `docs/laserdisc/`), subject to the
scan's copyright status, which is not established. This is a proposal only;
the submodule is not edited here.

### Isailović, *Videodisc and Optical Memory Systems* (1985)

J. Isailović, *Videodisc and Optical Memory Systems*, Prentice-Hall, 1985,
ISBN 0-13-942053-3. A Markdown transcription is in the
`analogue-video-specifications` submodule (commit `c3f6737`):
[Videodisc-and-Optical-Memory-Systems.md](../../analogue-video-specifications/docs/laserdisc/Videodisc-and-Optical-Memory-Systems/Videodisc-and-Optical-Memory-Systems.md).
Section numbers below are the book's.

**Caution.** The author worked at MCA DiscoVision, so the NTSC optical
material describes the format that IEC 60857 later standardised (§1.9.1's
carrier frequencies and pre-emphasis agree with IEC). Even so, much of the
book is a general survey covering several systems, and the transcription
contains errors: §2.3.1.4 gives 2.5 × 10⁻³ J per hole where 20 mW at
8 × 10⁶ holes/s is 2.5 × 10⁻⁹ J; Appendix 2.2 prints `E(u,0) = 4P(π d_eff s)`
for `4P/(π d_eff s)`. As with PTR, use it for **mechanisms, orders of
magnitude and bounds**, never for parameter values that IEC 60857 sets.

| Stage | Location | What it establishes |
|-------|----------|---------------------|
| S4, S5 | §1.9.1 | NTSC carriers 7.6 / 8.1 / 9.3 MHz, maximum instantaneous 11.3 MHz; pre-emphasis 320 ns / 120 ns. **Cross-checks** IEC 60857 |
| S6, S7 | §1.4, Fig. 1-10; §2.6.1 | Video and audio FM carriers are linearly summed then hard-limited; the 50 % duty cycle minimises the carrier's second harmonic; the photoresist threshold "acts as a (hard) limiter", so the laser drive need not be a clean pulse |
| S7 | §2.3.1.6 | Duty-cycle acceptance: on an 8.1 MHz carrier pulse-width modulated by 2.5 MHz, the IM products at f₀ ± fₐ appear when the average duty cycle departs from 50 %, and must stay below −29 dB, "between 40 and 60 %" duty cycle at playback (stated, not derived). Typical master at the inner radius: CNR 58 dB, IM −30 dB |
| S8 | §2.3.1.4 | Metal-film cutting: holes are the positive half-cycles of the FM signal, "the ends of the holes mark the zero crossings"; short on/off transitions make the hole ends insensitive to the melting threshold. Leadscrew advance 1.66 µm per turn |
| S8 | §2.3.1.2–2.3.1.3, Fig. 2-5 | Photoresist 1100–1500 Å thick on glass; minimum developed hole ≈ 0.4 µm; edges "distorted approximately 0.1 µm", causing "proportional and fixed-length asymmetry". Pit wall slope and duty cycle are set by exposure and development, adjusted empirically per master |
| S8 | App. 2.2 | **Exposure model:** `E(u,v) = (1/s)∫ I(u′) h(u − u′, v) du′`, with `s` the track speed. Two development thresholds, E₂ (etching starts) and E₁ (full depth), give the minimum and maximum pit widths (from the contour across the track) and lengths (along it); the difference sets the wall slope. Example: AZ 1350 needs 60 mJ/cm² for d_eff = 0.5 µm |
| S8 | §2.3.1.2, §2.6.1; §2.3.1 | CAV exposure ∝ 1/R, so laser power is raised in proportion to radius; recording spot `w = λ/(2NA)` at half intensity, NA ≈ 0.75 (§2.6.2: spot < 0.5 µm, focus held to ±0.1 µm) |
| S8 | §2.6 | The recorder may include an aperture corrector that keeps the black-level carrier exposure constant with radius and white-level amplitude equal to sync-tip amplitude. Group-delay pre-distortion belongs in mastering because "it is easier to compensate … once during recording than in each player" (agrees with S3) |
| S9 | §2.3.1.3 | Optimum pit depth for readout through the substrate `d ≥ λ/(4n)`: 103 nm for n = 1.54 (104 nm for the IEC n = 1.525) |
| S9 | §2.3.2–2.3.3, §2.5, §6.2 | Master → mother → submother → stamper → injection-moulded replica → aluminium (700–1500 Å) → overcoat. "Ideally, each replication step will be an exact reproduction" of the master. Departures, without numbers: bumps "from almost Gaussian to T shapes" (phase jitter); metallization blurs pit edges like a low-pass filter, and unequal blurring of the two edges gives jitter |
| S9 (dropouts) | §6.2.2, §6.5.2, §6.6 | Dropout lengths: one pit on metal-film masters, one to two pits on photoresist masters, up to about six pits for an inclusion in the plastic. Count against duration fitted by `a·e^(−bx)`, `a/x²` or `a·e^(−b√x)`; totals vary "by several orders" with plastic quality. Stamper 2–9× the master's dropout rate; replica about 4× the stamper's. In CAV the same particle gives a longer dropout at the inner radius; a uniform defect density gives a rate proportional to radius |
| S10 | §3.5; §3.7.1; App. 3.3 | 100 µm eccentricity at 55 mm gives a 10 µs timing error at 30 Hz; 5 ns is acceptable; higher-frequency timing error falls about 12 dB per octave; adjacent-track timing ±25 ns. **Cross-checks** §4.19. Eccentricity as `R(t) = R + ΔR cos ω_d t`, giving phase modulation of index `(f_s/f_d)(ΔR/R)` on a recorded tone |
| S11 | §3.1; §5.4.2, eqs 5-87, 5-88; §5.6, Fig. 5-38 | When one objective both illuminates and collects, the response "falls gradually" and cuts off at `2NA/λ` (Fig. 5-38); the diffraction-limited transfer is the overlap of two displaced pupils (eq. 5-87). §3.1 computes the cut-off at 55 mm as 13.2 MHz. NA 0.4 half-intensity spot 0.9 µm. **Confirms** the structure and cut-off of the S11 transfer function for a uniform pupil (the Gaussian weighting is not covered) |
| S11 | §3.2.1 | λ/4 pits: best contrast when the power returned from the pit equals that from the land (as PTR); λ/8 pits need differential detection |
| S11, S12 | §3.4, App. 3.1, App. 3.2; §6.3.2, eqs 6-15 to 6-18 | The readout MTF attenuates the FM sidebands unequally, which distorts the demodulated signal; in CAV it is radius-dependent, and at the inner radius chroma demodulates **larger** relative to luma. Players compensate with a cosine equaliser `1 − 2a cos ωτ` (example: τ ≈ 35 ns, peak boost at 14.4 MHz, a < 0.5), sometimes made radius-dependent from the burst amplitude |
| Phase 9 | §3.3; §6.2.1, §6.3, §6.5 | Disc noise dominates; photodiode noise is 10–20 dB below it; He-Ne laser noise white within 10 dB from dc to 20 MHz. Measured disc noise: approximately Gaussian, white before the readout MTF; carrier SNR about 60 dB in 30 kHz (about 35 dB in 10 MHz). Surface roughness is a signal-independent phase noise |

**Proposed upstream:** none; the transcription is already in the submodule.

## Not read

The order in which to acquire these, with links, is in
[source-acquisition.md](source-acquisition.md).

| Source | Access | Expected to cover | Resolves |
|--------|--------|-------------------|----------|
| G. Bouwhuis, J. Braat, A. Huijser, J. Pasman, G. van Rosmalen, K. Schouhamer Immink, *Principles of Optical Disc Systems*, Adam Hilger, 1985, ISBN 0-85274-785-3 | Loan only (archive.org controlled lending) | Chapters "Read-out of optical discs" (Braat), "Vector theory of diffraction" and "Mastering" (Pasman) | S11 transfer function and phase-pit model; S8 exposure; S9 pit geometry |
| H.H. Hopkins, "Diffraction theory of laser read-out systems for optical video discs", JOSA 69(1), 4–24 (1979), doi:10.1364/JOSA.69.000004 | Paywalled | λ/4 pits, fundamental and harmonic signal formulae, focus error and aberrations | S11 linear model confirmation; Task 10.1 phase-pit model; Task 8.4 focus |
| G. Bouwhuis & J. Braat, "Video disk player optics", Applied Optics 17(13), 1993–2000 (1978), doi:10.1364/AO.17.001993 | Paywalled | Readout objective, NA ≥ 0.4 for 12 MHz bandwidth, error signals | S11 |
| B.A.J. Jacobs, "Laser beam recording of video master disks", Applied Optics 17(13), 2001–2006 (1978), doi:10.1364/AO.17.002001 | Paywalled | Real-time recording on positive photoresist with an electro-optic modulator | S8 exposure model, pit length bias |
| J.P.J. Heemskerk, "Noise in a video disk system: experiments with an (AlGa)As laser", Applied Optics 17(13), 2007–2012 (1978), doi:10.1364/AO.17.002007 | Paywalled | Noise from light source, medium and detection electronics, against light level | Phase 9 noise |
| C.H.F. Velzel, "Laser beam reading of video records", Applied Optics 17(13), 2029–2036 (1978), doi:10.1364/AO.17.002029 | Paywalled | Partially coherent imaging of a thin phase object; signal amplitudes, intermodulation, crosstalk | S11; Phase 9 crosstalk; Task 10.1 |
| A. Korpel, "Simplified diffraction theory of the video disk", Applied Optics 17(13), 2037–2042 (1978), doi:10.1364/AO.17.002037 | Paywalled | Semi-quantitative readout model | S11 cross-check |
| J.H.T. Pasman, J.F. Olijhoek, W. Verkaik, "Developments in optical disk mastering", Proc. SPIE 529, p. 62 (1985), doi:10.1117/12.946433 | Paywalled | Mastering | S8, S9 |
| J.F. Olijhoek, T.H. Peek, C.A. Wesdorp, "Mastering technology for the Philips optical disc systems", Electro '81 | Citation only | Mastering | S8, S9 |
| P.F. Panter, *Modulation, Noise, and Spectral Analysis*, McGraw-Hill, 1965, p. 542 | Loan only | Symmetrical double-edge PWM, cited by IEC 60857 §7.2.1 | S6 (audio, EFM) |
| Player service manuals | Varies | RF test-point response, player RF low-pass filter | S12 player profiles |

None of these is needed to implement the ideal disc. Each `unsourced-ideal`
row in the [parameter register](parameter-register.md) names its candidate
here.
