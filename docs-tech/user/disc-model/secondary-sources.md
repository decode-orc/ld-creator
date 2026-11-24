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

### Braat, Dirksen & Janssen, "Diffractive read-out of optical discs"

J. Braat, P. Dirksen, A.J.E.M. Janssen, "Diffractive read-out of optical
discs", book chapter, author's version dated 2008 (PDF title
"FocusonMicroscopyBraatetalVersion2008"):
[nuhagphp.univie.ac.at/janssen/data/p119.pdf](https://nuhagphp.univie.ac.at/janssen/data/p119.pdf).
Braat wrote the readout chapter of Bouwhuis et al. (1985). The chapter is
mostly about CD and DVD, but its readout theory applies to any reflective
disc. Section numbers below are the chapter's.

| Stage | Location | What it establishes |
|-------|----------|---------------------|
| S11 | §3.1, Figs 4–6 | Reflective readout is a scanning microscope in which one objective illuminates and collects, so the coherence factor γ = NA_D/NA = 1. Maximum detectable frequency `2NA/λ`; in practice detectable to 90–95 % of it, with electronic equalisation. **Confirms** the S11 cut-off and the incoherent-limit (pupil-overlap) form |
| S11 | §6.2, eqs 17–21 | The detector signal in Hopkins's formulation (the section states it "uses the material presented in" Hopkins 1979 and Bouwhuis et al. 1985): a sum over diffracted orders `ρ_m ρ*_m′` times an overlap integral of displaced pupil functions. The incident "lens function" `f` carries "pupil shape, gaussian filling factor of the incoming beam" and aberrations. **Confirms** that the Gaussian filling enters the transfer function through the incident pupil amplitude |
| S11 | §6.2, before eq 18 | For a reflective system the collecting lens function is set to `g(X′,Y′) = f(−X,−Y)`. Read literally, that applies the Gaussian weighting on the return path too. Physically the Gaussian is the incoming beam's profile, not a transmission of the lens, so S11 applies it once (incident) with a uniform collecting aperture. The chapter does not settle this; see the S11 open item |
| S11 (Task 10.1) | §6.2–6.3, eqs 18–23 | Harmonic-coefficient method for periodic (synthetic-period) pit patterns, with the imaging integral computed once and reused. A candidate method for the phase-pit model |

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

### Open-access sources from the wider search

Found by searching for the missing information rather than for specific
titles (November 2026). Every source was opened and the quotes are from its
text. The S11 entry (van de Nes et al.), the PTR pit dimensions and the
US 4,456,914 threshold rule were re-checked against the text before being
recorded.

| Stage | Source | Location | What it establishes |
|-------|--------|----------|---------------------|
| S11 | A.S. van de Nes, J.J.M. Braat, S.F. Pereira, "High-density optical data storage", *Rep. Prog. Phys.* 69, 2323–2363 (2006). [TU Delft repository](https://repository.tudelft.nl/file/File_02b1f8da-23fc-4e6a-9b5d-14fad73174f9) | §2.3, eqs (4)–(7), pp. 2332–2333 | `A(X,Y)` is the Fourier transform of the scanning spot, that is the incident pupil field (a top-hat only "in the case of a perfect diffraction-limited scanning spot"). "One collects all the light captured within the exit pupil of the objective on the way back from the disc. The detector current is then proportional to iD ∝ ∫ \|B(X,Y)\|² dXdY", with no further weighting. So the beam profile enters the transfer function **once**, through `A`. **Resolves** the S11 pupil-weighting question in favour of the S11 model |
| S11 | M. Kondo, "Truncation coefficient optimization for elliptical laser Gaussian beams on optical head", *Rev. Laser Eng.* 21(6), 634 (1993). [J-STAGE](https://www.jstage.jst.go.jp/article/lsj1973/21/6/21_6_634/_pdf) | p. 636, eqs (8)–(11) | Optical-head model citing Hopkins 1979: the truncated Gaussian sets the focal field, and the reflected light is integrated uniformly over the objective aperture. Agrees with the entry above |
| S8 | US 4,456,914 (MCA DiscoVision). [Google Patents](https://patents.google.com/patent/US4456914A/en) | Description | LaserVision recorder: argon-ion laser, "0.75 NA" objective, "0.8 micron diameter" recording spot, focus "plus or minus 0.3 micron", "2.00 microns per turn". "The peak intensity of the writing beam is adjusted so that one-half of the peak intensity is equal to the threshold power level", held by "an average intensity biasing servo or a second harmonic biasing servo" toward a "50/50 duty cycle" |
| S8 | US 4,422,904 (MCA DiscoVision). [Google Patents](https://patents.google.com/patent/US4422904A/en) | Description | Resist "Shipley AZ 1350" thinned 3:1, "about 1150 to 1350 Å"; developer of "about 0.14 to 0.16" normality for "about 60 seconds" gives "unexpectedly steeper sidewalls". No wall angle |
| S8 (analogous) | US 6,728,196 B2 (Imation). [Google Patents](https://patents.google.com/patent/US6728196B2/en) | Description | CD/DVD-era recorder: Gaussian spot "SS=0.57λ/NA (FWHM)"; resist development rate `R = R₀(1−M)^q + R_b`, "Typical values… q=3, 10<R₀<200 [nm/sec]", depth dependence of exposure negligible for thin resist. No exposure constant for AZ 1350 |
| S9 | Philips Technical Review 39(2), p. 37 (1980). [worldradiohistory.com](https://www.worldradiohistory.com/Archive-Company-Publications/Philips-Technical-Review/) | p. 37 | "pitch of 1.66 µm and consisting of pits 0.6 µm wide and 0.12 µm deep" |
| S9 | Haverkorn van Rijsewijk, Legierse, Thomas, Philips Technical Review 40(10), pp. 287–297 (1982). Same archive | pp. 287–288, Fig. 2 | "small pits 0.12 µm deep and 0.4 µm wide", lengths "between about 0.5 and 2.0 µm", pitch "1.6 µm to 2.0 µm"; SEM of a production disc. The European discs (Mullard, Blackburn) are made by the **2p photopolymerisation process**, not injection moulding; Al mirror about 60 nm. Repeated in PTR 42(5) p. 150 (1985) |
| S9 | US 4,500,392 (MCA DiscoVision). [Google Patents](https://patents.google.com/patent/US4500392A/en) | Description | Stamper bumps "approximately 0.6 microns wide" and "on the order of 0.15 microns in height"; chrome plating gives "a rounding off of the bumps" (no numbers). Dropouts on a deliberately poor stamper: "approximately 260 dropouts per frame", background "about 15 counts per frame"; after chroming "about 50 dropouts per frame… on the borderline of being acceptable" |
| S12 | Pioneer LD-V4300D Service Manual. [domesday86.com](https://www.domesday86.com/wp-content/uploads/2018/09/Pioneer-LD-V4300D-Service-Manual-DD86_2.pdf) | PDF pp. 23, 24, 93, 94 | The DdD reference player's RF chain, recorded in [S12](s12-player-front-end.md#sources) |
| S6, S7 | US 4,683,502 (Pioneer). [Google Patents](https://patents.google.com/patent/US4683502A/en) | Description | The limiter output "is cut around the zero crossing level… to obtain a pulse-width-modulated signal"; "the level of the audio subcarriers is usually set at around one tenth the video carrier level, and may be further lowered by 6 dB by passage through the limiter" |
| S6 | US 3,911,476 (RCA). [Google Patents](https://patents.google.com/patent/US3911476A/en) | Description | The limited sum of picture and sound carriers is a pulse train with "a duty cycle which varies cyclically about a 0.5 value" at the sound-carrier frequency, with components at `2f_p ± f_s`. RCA CED, same mechanism |
| S6 (analogous) | J. Sun, "Pulse-width modulation", ch. 2 in Vasca & Iannelli (eds), *Dynamics and Control of Switched Electronic Systems*, Springer 2012. [Free PDF](https://www.elektronika.ftn.uns.ac.rs/racunarska-elektronika/wp-content/uploads/sites/21/2018/03/2012-BkCh-PWM.pdf) | Eqs (2.15), (2.17), Table 2.1 | Spectrum of naturally sampled double-edge PWM: sideband at `m·f_c + n·f₁` has amplitude `(2/mπ)·J_n(mπM/2)`. Derived for a triangle carrier; LaserVision limits a sinusoidal carrier, so this gives the form, not the LaserVision values. Substitute for Panter p. 542 |
| S7, S8 | US 3,988,532 (Zenith, Korpel). [Google Patents](https://patents.google.com/patent/US3988532A/en) | Description | A duty-cycle error gives spurious components with "a triangular amplitude-frequency characteristic with a slope of 6 db/octave", whose amplitude "is proportional, both in sign and magnitude, to the percentage error in duty cycle" |
| Phase 9 | R. Badger, J. Allen (Pioneer Video), "The audio side of the laser videodisc", AES 72nd Convention, preprint 1935 (1982). [PDF](https://vidachestvo.ru/images/1/19/Pioneer._The_Audio_Side_of_the_Laser_Videodisc.pdf) | §4, pp. 6–8 | "Typical discs have peak to unweighted peak video carrier to noise ratios (VCNR) between 58 and 63 db, producing a video signal to noise ratio between 38 and 43 db"; "ACNR and VCNR vary slightly depending on disc format and the track radius". Measurement bandwidth not stated. Wow and flutter "typically below .1%" |
| S10 | US 4,422,103 and US 4,425,583 (Universal Pioneer). [Google Patents](https://patents.google.com/patent/US4422103A/en), [Google Patents](https://patents.google.com/patent/US4425583A/en) | Description | Time-base variation "as much as 20 μs… during one revolution"; requirement "10 ns or less"; after the tangential mirror, residual "no more than 1 μs" |

### Approximate figures from the final search

Where no LaserVision figure exists, these give an exact figure for a
neighbouring case (another wavelength, system or disc type) or a design
range. Each row says how it carries over. Quotes were read in the source;
figure readings are by eye and good to about ±2 dB. Values marked
*derived* are worked out here, not stated by the source.

| Stage | Source | Location | Figure | Carry-over to LaserVision |
|-------|--------|----------|--------|---------------------------|
| S11 | H. Satoh, K. Ikeda, Y. Kinoshita, T. Odaka, O. Sasaki (Toshiba), 「高精細ビデオディスクの信号伝達特性」 (Signal characteristics of high-definition video discs), *J. ITE Japan* 44(10), 1376–1382 (1990). J-STAGE | p. 1377; Fig. 2 p. 1378; p. 1379 | 670 nm, NA 0.6, 1.16 µm pitch. Aberration-free calculated signal about −7 dB at 2 MHz and −17 to −18 dB at 15 MHz (Fig. 2); measured "−16から−23dB" over 11–15 MHz, "約3dB低い" than the calculation | 15 MHz is `ν/fc` = 0.62, close to the 802 /mm point (0.634). Calculated roll-off 2→15 MHz about −10.5 dB; S11 model −12.2 dB (−10.6 dB uniform). Agrees within the reading error; cannot separate uniform from Gaussian-once |
| S11 | Goodman, *Introduction to Fourier Optics*, circular-pupil incoherent OTF | — | Uniform pupil `(2/π)[acos x − x√(1−x²)]` = 0.2507 at x = 0.634 (*derived*) | Matches the S11 integration for a uniform pupil (0.2507), so the method is right; the Gaussian-once value 0.2136 follows from the same integration |
| S12 | Sony CXA1753M "I-V Photo IC for CD Player", datasheet E93537A79-TE | p. 2 | −3 dB 2.5–3.0 MHz typ | CD-only class; too slow for the 7.6–9.3 MHz carrier, so a lower bound on what PA0037 is *not* |
| S12 | N. Fukunaga, M. Yamamoto, M. Kubo, N. Okabayashi (Sharp), "Si-OEIC (OPIC) for optical pickup", *IEEE Trans. Consum. Electron.* 43(2), 157–164 (1997), doi:10.1109/30.585534 | Abstract only | Cut-off 18.4 MHz at 780 nm, 21.1 MHz at 635 nm | PA0037 must pass at least the 13–14 MHz inner-radius cut-off; parts of this era suggest a −3 dB point of roughly 15–25 MHz (*derived*). As a single pole at 15–20 MHz: a few tenths of a dB at 8–9 MHz |
| Phase 9 crosstalk | M. Kondo (1993), as above | Abstract; Figs 3, 4b pp. 638–639 | 780 nm, NA 0.5: "minimum cross talk level was −40 dB" at 1.4 µm spacing; about −50 to −55 dB at 1.65 µm; slope about −37 dB/µm. "The average information track spacing in CD and LD is 1.6 to 1.67 µm" | `λ/NA` is 1.56 µm there and 1.58 µm for LaserVision, so 1.67 µm pitch ≈ 1.65 µm in Kondo: about −50 dB on track, about +4 dB worse per 0.1 µm off-track (*derived*). Long pits, elliptical beam, truncation below LaserVision's |
| Phase 9 crosstalk | Satoh et al. (1990), as above | p. 1379; Figs 6–9 pp. 1379–1380 | At the carrier: calculated "−42dB以下" and measured "−40dB以下" relative to the read signal at 1.16 µm pitch. Pit width 0.2–0.4 µm and ±10 % depth change it by under 1 dB. Not monotonic in pitch (sidelobes) | `pitch·NA/λ` is 1.04 there and 1.06 for LaserVision: about −40 dB relative to the carrier on track |
| Phase 9 noise | IEC 60857 Amendment 2 §13.1.5, Figure 25 | Figure notes | "RBW 30 kHz VBW 30 kHz" for RF spectrum levels | The only LaserVision document naming an RBW. Badger & Allen's 60 dB VCNR read in 30 kHz gives 34.8 dB in 10 MHz (*derived*), matching Isailović §6.5.1's "about 35 dB in 10 MHz"; so 30 kHz is taken as their bandwidth (inferred) |
| Phase 9 noise | BBC RD 1986/01 Fig. 9(f), "Spectrum off flat silvered disc – no data recorded (noise)" | Fig. 9(f) | About −13 to −20 dB (arbitrary reference) from 0 to 5 MHz, then about −2 dB/MHz to −63 dB at 24 MHz; frequency scale assumed 5 MHz/div from Figs 9(a)/(b) | Disc noise through a real LaserVision pickup falls with the readout MTF, reaching the `2NA·v/λ` cut-off near 20–25 MHz: consistent with spatially white roughness filtered by S11. Then the temporal noise density scales as 1/v, about 4.2 dB higher at 55 mm than at 145 mm (*derived*, unsourced) |
| Phase 9 noise | Isailović 1985 §6.5.1, Figs 6-11, 6-14 | §6.5.1 | Noise slope about 0.7–0.9 dB/MHz (schematic lines). "The slope of the curve can be considered to be the result of the MTF of the read lens" | Trend only |
| Phase 9 noise | M. Ojima, A. Arimoto, N. Chinone, T. Gotoh, K. Aiki (Hitachi), "Diode laser noise at video frequencies in optical videodisc players", *Appl. Opt.* 25(9), 1404–1410 (1986), doi:10.1364/AO.25.001404. [Abstract](https://opg.optica.org/ao/abstract.cfm?uri=ao-25-9-1404) | Abstract | RIN limit below 6×10⁻¹⁴ Hz⁻¹ (−132.2 dB/Hz) in a practical player | For a diode-laser player. Laser-limited C/N about 78 dB in 30 kHz at modulation 0.5 (*derived*), about 18 dB below disc noise. No HeNe RIN found (the LD-V4300D uses a diode) |
| S10 | Isailović 1985 §3.7.4, Fig. 3-47 "Tangential open loop gain" | Fig. 3-47 | 50 dB plateau from about 3 Hz, 70 dB peak at the 30 Hz rotation rate, −12 dB/oct above the mirror resonance, 0 dB at a few kHz; band "Statistical amplitude of time errors" falling about −12 dB/oct to about 1 kHz. "Bandwidth… limited in practice to approximately 1.6 kHz" | Residual = error/(1+G). 10 µs at 30 Hz → about 3 ns after 70 dB. If the −12 dB/oct envelope is anchored at 10 µs at 30 Hz: 2.5 µs at 60 Hz, 0.63 µs at 120 Hz, about 10 ns at 1 kHz before correction (*derived*; the anchor is not stated) |
| S10 | US 4,005,260 (US Philips, 1977). [Google Patents](https://patents.google.com/patent/US4005260A/en) | Description | Mirror response flat below "a first limit frequency, corresponding to the speed of the record carrier", then "-12 dB/octave"; line frequency "strongly damped" | Shape only |
| S10 | US 4,468,709 (Olympus, 1984). [Google Patents](https://patents.google.com/patent/US4468709A/en) | Description | CCD TBC "variable delay time from 35 μsec. to 70 μsec." (±17.5 µs); residual "about 80 nsec" with H-sync control, "about 5 nsec" with burst | An electronic TBC's range and residual for an optical videodisc player |
| S10 | US 4,425,583 (Universal Pioneer), as above | Description | Mirror errors "can occur at the repetition rate of the vertical retrace time (60 Hz…)" | A 60 Hz residual line from the burst-less vertical interval |
| S8 | F.H. Dill, J.M. Shaw, "Thermal effects on the photoresist AZ1350J", *IBM J. Res. Dev.* 21(3), 210–218 (1977). [bitsavers](http://bitsavers.trailing-edge.com/pdf/ibm/IBM_Journal_of_Research_and_Development/213/ibmrd2103B.pdf) | p. 212; Figs 1, 2 | At 404.7 nm, 70 °C prebake: A = 0.89 µm⁻¹, B = 0.077 µm⁻¹, C printed as 0.28 cm²/mJ. Development rate about 100 nm/s fully exposed to about 0.1 nm/s at M = 1 (Fig. 2, log scale) | Wrong wavelength (the recorder used about 458 nm, below). C may be a ×10 misprint: Fig. 1's bleaching time fits about 0.028 (*derived*) |
| S8 | D.J. Kim, *Characterization and Modeling of Positive Photoresist*, PhD dissertation, UC Berkeley, UCB/ERL M84/65 (1984). [PDF](https://www2.eecs.berkeley.edu/Pubs/TechRpts/1984/ERL-m-84-65.pdf) | App. C Table IV p. 127; eq. 3.2.4 p. 44; p. 4 | 1350J at 436 nm, 70 °C prebake: A = 0.55 µm⁻¹, B = 0.045 µm⁻¹, C = 0.013 cm²/mJ; `R = R1(1 − M·e^{−R3(1−M)}) + R2·M·e^{−R3(1−M)}`, R3 = 7.4 (R1 about 0.2 µm/s; R1, R2 last digits illegible). Unexposed "below 1 nm/sec", exposed "above 200 nm/sec" | The nearest measured set to the recorder line. Absorption at 458 nm is lower, so A and the clearing dose differ |
| S8 | A.H. Firester et al., "Optical recording techniques for the RCA VideoDisc", *RCA Review* 39(3), 427–457 (1978). [PDF](https://www.americanradiohistory.com/ARCHIVE-RCA/RCA-Review/RCA-Review-1978-Sep.pdf) | pp. 437, 445, 452 | AZ1350B, He-Cd 442 nm: development nonlinearity γ = 2 to 2.5; R1 = 100–200 nm/min, erosion R2 = 2–10 nm/min, "typical VideoDisc recording conditions" | Same resist family and a similar line; model inputs given as typical, not fitted |
| S8 | US 4,152,726 (Sony, 1979). [Google Patents](https://patents.google.com/patent/US4152726A/en) | Description | Video master at 1800 rpm, one NTSC frame per turn: "argon gas laser… wavelength of 4579 angstroms" on "AZ-1350J" 1.0 µm thick | Recorder line: argon 457.9 nm. DiscoVision (US 4,456,914) says only "argon ion laser"; the Philips LHH0400 CD master recorder also uses 457.9 nm (product literature, [dutchaudioclassics](https://dutchaudioclassics.nl/philips_lhh0400-cdmaster-recording-system/)). Isailović App. 2.2's 60 mJ/cm² clearing dose is at 350 nm (krypton) |
| S9 | US 4,230,915 (US Philips; Dil, Jacobs; priority 1978). [Google Patents](https://patents.google.com/patent/US4230915A/en) | Description, Figs 2, 3, 7 | Wall angle, from the normal to the surface: "of the order of 45° to 50°… acceptable… 30° to 65°". `w_eff = w − d_g·tanθ` (width at the land plane minus the slope), example w = 625 nm, θ = 45°, d_g = 135 nm → 490 nm. Width "at the inner area… for example 800 nm, at the outer area, for example 500 nm" (per US 4,118,734, variable-width tracks) | A Philips design value, not a measurement. With 0.12 µm depth and 45°, top and floor widths differ by 0.24 µm: 0.6 µm at the land is about 0.48 µm at half depth (*derived*). The 0.4/0.6 µm conflict may be width measured at different heights or radii |
| S9 | US 4,930,116 (US Philips). Google Patents | Description | Repeats 45–50°; "the wall steepness increases with an increase of the developing time" | Links wall angle to S8 development time |
| S9 (analogous) | JVC US 4,655,876; Toshiba US 5,459,712. Google Patents | Description | Glass master pits about 0.1 µm deep, walls "45 degrees with the horizontal" (JVC); "actual pit in the range of 30° to 60°" (Toshiba, DVD era) | Same range as the Philips patent |
| S9 | Kloosterboer, Lippits, Meinders, "Photopolymerizable lacquers for LaserVision video discs", PTR 40(10), 298–309 (1982). [PDF](https://www.americanradiohistory.com/Archive-Company-Publications/Philips-Technical-Review/80s/Philips-Technical-Review-1982.pdf) | p. 302; p. 290 | The base diacrylate gives "strong polymerization shrinkage, amounting to about 15% of the volume"; production mixes add a low-shrinkage monomer. 2p cures at room temperature, so "no thermal shrinkage after curing" | 2p (European discs): at most about 5 % (isotropic) to 15 % (all normal to the layer) of the 120 nm relief, 6–18 nm, as an upper bound (*derived*) |
| S9 (analogous) | C. Hougum et al. (GE Plastics), "Addressing issues in the manufacture of optical discs", *SVC 39th Annual Technical Conference Proceedings*, 313–315 (1996). [PDF](https://www.svc.org/clientuploads/directory/resource_library/96_313.pdf) | Table 1 | CD stamper bumps 157.2 nm (30 mm) and 150 nm (50 mm); moulded pit depths 107.5–148.1 nm over six conditions: 72–96 % depth transfer, poorer at the outer radius except when optimised | Injection moulding (NTSC discs): 5–25 % depth loss plausible; LaserVision's wider, shallow pits fill more easily (*derived*) |
| S13 | TI ADS825 datasheet, SBAS069B (2002). [ti.com](https://www.ti.com/lit/gpn/ADS825) | pp. 2–4, 8–9, 12 | Recorded in [S13](s13-rf-capture.md#open-items) | Exact |
| S13 | Cyclone IV Device Datasheet CYIV-53001-1.3 (2010), Table 1–25 | p. 1–24 | PLL regular-I/O output jitter 75 mUI peak-to-peak (10⁻¹² probability, "14 sigma") for FOUT < 100 MHz | At 80 MHz, about 0.94 ns p-p, about 67 ps rms (*derived*, worst-case bound) |
| S13 (analogous) | ECS-3225MV and SiTime SiT8008 oscillator datasheets | Stability tables | 3.3 V SMD 50 MHz class: ±20 to ±100 ppm total stability, ±50 ppm typical option | DE0-Nano Y1 has no part number in Terasic's schematic ("50MHZ" only); ±50 ppm adopted as approximate |

**Not found in any accessible source, even approximately:** a measured
LaserVision pit wall angle or width profile; AZ 1350 constants at 457.9 nm;
pit-scale change from electroforming; a measured LaserDisc noise spectrum
against radius; magnitudes at harmonics of the rotation rate; the PA0037
datasheet. Each is measurable (a DdD capture for noise, timing and the
OEIC; an AFM of a disc for the pit).

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
