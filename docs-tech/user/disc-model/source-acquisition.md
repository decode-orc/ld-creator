# Sources to acquire

The documents the disc model still needs, most important first. Each entry
names the stage and open item it resolves, and the plan task that is blocked
until it is read. Sources already read are listed in
[secondary-sources.md](secondary-sources.md). When a document here has been
read, its findings go there and its entry is removed from this list.

## Priority 1: needed before Phase 5 (ideal disc)

These sources affect the ideal disc's RF. Without them, an inner-radius
round-trip failure can't be attributed to ld-decode or to the model
([metrics](../ntsc-round-trip-metrics.md#readout-mtf-and-decoder-settings)).

### 1. Hopkins (1979): readout diffraction theory

- **Citation:** H.H. Hopkins, "Diffraction theory of laser read-out systems
  for optical video discs", *J. Opt. Soc. Am.* 69(1), 4–24 (1979).
- **Link:** [doi:10.1364/JOSA.69.000004](https://doi.org/10.1364/JOSA.69.000004)
  (Optica Publishing Group; paywalled).
- **Resolves:**
  - S11 open item: the Gaussian pupil weighting of the transfer function
    (the 23 % rim intensity of IEC §11.1). Isailović §5.4.2 confirms the
    pupil-overlap structure and the `2NA/λ` cut-off, but only for a uniform
    pupil ([S11](s11-optical-readout.md#open-items)).
  - Register row `S11.model`.
  - Later: plan Task 10.1 (phase-pit model, the upper Figure 18 band) and
    Task 8.4 (focus error).
- **Why first:** S11's transfer function sets the inner-radius MTF that
  ld-decode's compensation is tested against (+2.7 dB chroma at 55 mm
  uncompensated).

### 2. Bouwhuis et al. (1985): *Principles of Optical Disc Systems*

- **Citation:** G. Bouwhuis, J. Braat, A. Huijser, J. Pasman, G. van
  Rosmalen, K. Schouhamer Immink, *Principles of Optical Disc Systems*, Adam
  Hilger, Bristol, 1985. ISBN 0-85274-785-3.
- **Access:** [archive.org search](https://archive.org/search?query=principles+of+optical+disc+systems)
  (controlled lending), interlibrary loan (for example
  [TAMU catalogue](https://libcat.library.tamu.edu/Record/in00000384795)), or
  secondhand.
- **Chapters and what they resolve:**
  - "Read-out of optical discs" (Braat): the S11 transfer function, as an
    alternative to Hopkins (item 1). Phase 5.
  - "Vector theory of diffraction": plan Task 10.1.
  - "Mastering" (Pasman): S8 cutting spot `h`, thresholds E₁ and E₂, pit
    length bias; S9 pit geometry and replication blur. Phase 8.
- **Why second:** one book covers S11 now and S8/S9 later, so it's the best
  single purchase. If item 1 isn't available, this chapter is the
  alternative for S11.

### 3. LD-V4300D front end (reference player)

The DomesdayDuplicator's reference player is the Pioneer LD-V4300D
([DdD docs, LD-V4300D overview](https://github.com/simoninns/DomesdayDuplicator/blob/main/docs/content/ldv4300d/overview.md)).
The service manual is **already available**:

- **Service manual:** [Pioneer LD-V4300D Service Manual (domesday86.com)](https://www.domesday86.com/wp-content/uploads/2018/09/Pioneer-LD-V4300D-Service-Manual-DD86_2.pdf),
  101 pages, scanned with OCR.
- **RF tap:** [DdD docs, LD-V4300D RF output](https://github.com/simoninns/DomesdayDuplicator/blob/main/docs/content/ldv4300d/rf-output.md).
  The tap is TP1 pin 1 ("RF") on the FTSB assembly, a 50 Ω test point.
- **RF level:** service manual §10 "RF gain adjustment" (PDF p. 85) and the
  [DdD calibration guide](https://github.com/simoninns/DomesdayDuplicator/blob/main/docs/content/ldv4300d/calibration.md):
  VR1 on FTSB sets 300 mV p-p at TP1-1 (GGV1003/GGV1069, frame 15000).

**What the service manual shows (read for this list).**

- §9.2 FTS block diagram (PDF p. 93): the pickup's OEIC (photodiode with
  integrated amplifier) "RF" output passes two transistor stages, with VR1
  "RF GAIN", to the FTSB "RF" output, which is TP1-1.
- §9.3 video block diagram (PDF p. 94): the RF then goes to the VDEM
  assembly. There it passes an amplifier, an "RF COR" block, an audio FM
  trap and a low-pass filter before the video demodulator (IC401 PA5013A).
  The RF COR block is fed by an "RF FREQ" control line from the CONT
  assembly.
- §5.5 VDEM schematic (PDF p. 34): the RF enters at CN21, passes the
  Q431–Q434 RF buffer and amplifier, then an LC network switched by Q401
  ("RF FILTER SW") before buffer Q404.

**Conclusion:** the DdD captures upstream of the player's RF correction and
filtering. On the reference player, the captured RF is unequalised, and that
is now sourced rather than inferred. This supports S12's default
(`mtf_equaliser: none`).

**Still open:**

- The frequency response of the OEIC and the FTSB RF output stage, up to the
  optical cut-off. The manual gives the circuit but no response, and the
  OEIC part has no datasheet located yet.
- What RF COR does and how RF FREQ drives it (radius-dependent MTF
  compensation, or a CAV/CLV switch). This is a decoder-side function and
  isn't needed for the ideal disc. Record it when player profiles are
  written (plan Task 9.3).

### 4. Bouwhuis & Braat (1978): player optics

- **Citation:** G. Bouwhuis, J. Braat, "Video disk player optics", *Applied
  Optics* 17(13), 1993–2000 (1978).
- **Link:** [doi:10.1364/AO.17.001993](https://doi.org/10.1364/AO.17.001993)
  (paywalled).
- **Resolves:** a cross-check of S11 (objective design, NA ≥ 0.4 for 12 MHz
  bandwidth). Not needed if item 1 or 2 is read.

### 5. ADS822/ADS825 datasheet

- **Citation:** Texas Instruments, *ADS822/ADS825 10-Bit, 40MHz Sampling
  Analog-to-Digital Converter*, SBAS069B.
- **Link:** [ti.com/lit/gpn/ADS825](https://www.ti.com/lit/gpn/ADS825) (free).
- **Resolves:** S13 open item: code transition points and full-scale
  accuracy for the DdD capture profile (plan Task 5.3). The `ideal` capture
  profile doesn't need it.

## Priority 2: Phase 8 (deterministic disc impairments)

The four entries below are all from the July 1978 Philips special issue of
*Applied Optics* 17(13), or from Philips authors. One library request can
cover the Applied Optics papers (here and in Priority 3).

### 6. Jacobs (1978): laser beam recording

- **Citation:** B.A.J. Jacobs, "Laser beam recording of video master
  disks", *Applied Optics* 17(13), 2001–2006 (1978).
- **Link:** [doi:10.1364/AO.17.002001](https://doi.org/10.1364/AO.17.002001)
  (paywalled).
- **Resolves:** S8 values for the cutting spot `h`, thresholds E₁ and E₂,
  and the pit length bias (register rows `S8.edge_profile`,
  `S8.length_bias`; plan Task 8.2). The model form is already sourced from
  Isailović App. 2.2.

### 7. Pasman, Olijhoek & Verkaik (1985): mastering

- **Citation:** J.H.T. Pasman, J.F. Olijhoek, W. Verkaik, "Developments in
  optical disk mastering", *Proc. SPIE* 529, 62 (1985).
- **Link:** [doi:10.1117/12.946433](https://doi.org/10.1117/12.946433)
  (paywalled).
- **Resolves:** S8 and S9 as item 6 (Tasks 8.2, 8.3).

### 8. Legierse & Pasman (1989): mastering and replication materials

- **Citation:** P.E.J. Legierse, J.H.T. Pasman, "Materials for Mastering and
  Replication Processes in Optical Video, Audio and Data Disc Production",
  Plenum Press, New York, 1989.
- **Link:** [Springer chapter](https://rd.springer.com/chapter/10.1007/978-1-4613-0843-0_11)
  (paywalled).
- **Resolves:** S9 pit width, wall slope and replication blur (register rows
  `S9.copy`, `S9.pit_geometry`; plan Task 8.3). Isailović gives the
  mechanisms but no magnitudes, and his widths conflict (0.4 µm and 0.8 µm).

### 9. Put (1990): trends in mastering (substitute)

- **Citation:** Put, "Trends in Optical Disk Mastering", *MRS Bulletin*
  (1990). Author's initials not yet confirmed.
- **Link:** [Cambridge Core](https://www.cambridge.org/core/journals/mrs-bulletin/article/trends-in-optical-disk-mastering/82495129C6F2B64BF71000BF92BA01FA)
  (paywalled).
- **Resolves:** a possible substitute for items 7 and 8 if they can't be
  obtained. Read only if needed; check the author's full name on the
  publisher page.

## Priority 3: Phase 9 and 10 (noise, crosstalk, readout refinement)

### 10. Heemskerk (1978): noise

- **Citation:** J.P.J. Heemskerk, "Noise in a video disk system:
  experiments with an (AlGa)As laser", *Applied Optics* 17(13), 2007–2012
  (1978).
- **Link:** [doi:10.1364/AO.17.002007](https://doi.org/10.1364/AO.17.002007)
  (paywalled).
- **Resolves:** noise from source, medium and detector against light level
  (plan Task 9.1). Isailović §6 already gives the shape (disc noise
  dominant, Gaussian, white before the MTF).

### 11. Velzel (1978): partially coherent readout

- **Citation:** C.H.F. Velzel, "Laser beam reading of video records",
  *Applied Optics* 17(13), 2029–2036 (1978).
- **Link:** [doi:10.1364/AO.17.002029](https://doi.org/10.1364/AO.17.002029)
  (paywalled).
- **Resolves:** intermodulation and crosstalk (plan Task 9.2); phase-pit
  readout (Task 10.1).

### 12. Korpel (1978): simplified diffraction theory

- **Citation:** A. Korpel, "Simplified diffraction theory of the video
  disk", *Applied Optics* 17(13), 2037–2042 (1978).
- **Link:** [doi:10.1364/AO.17.002037](https://doi.org/10.1364/AO.17.002037)
  (paywalled).
- **Resolves:** an independent cross-check of S11 and Task 10.1.

### 13. DE0-Nano user manual

- **Citation:** Terasic, *DE0-Nano User Manual*.
- **Link:** [Terasic DE0-Nano page](https://www.terasic.com.tw/cgi-bin/page/archive.pl?No=593)
  (free with registration).
- **Resolves:** S13 open item: the oscillator frequency tolerance, for a
  later clock-error impairment.

## Priority 4: outside the video work

### 14. Panter (1965): PWM spectrum

- **Citation:** P.F. Panter, *Modulation, Noise, and Spectral Analysis*,
  McGraw-Hill, 1965, p. 542.
- **Access:** interlibrary loan or archive.org lending.
- **Resolves:** S6 open item: symmetrical double-edge PWM, cited by IEC
  60857 §7.2.1. Needed for audio and EFM, not for video.

## Not needed

- **Further player service manuals.** The reference player is the
  LD-V4300D (item 3). Other players are needed only if plan Task 9.3 adds
  their profiles.
- **Olijhoek, Peek & Wesdorp, "Mastering technology for the Philips optical
  disc systems", Electro '81.** Citation only; items 6–8 cover the same
  ground.
