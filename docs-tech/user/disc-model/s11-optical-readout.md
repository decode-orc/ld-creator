# S11 — Optical readout (`optical_readout`)

## Physical process

A focused laser spot scans the track through the transparent substrate. The
pits are shallow reliefs in the reflective layer; light reflected from a pit
and from the surrounding land interferes, so less light returns into the
objective when the spot is over a pit. The reflected beam falls on a
photodiode, whose current follows the pit pattern blurred by the spot.

## Sources

- IEC 60857-1986 §11.1: λ = 632.8 nm; NA = 0.40 ± 0.01; circular beam with a
  Gaussian intensity profile whose rim intensity is 23 ± 1 % of the centre;
  first diffraction ring < 2 % of the central peak.
- §11.2: "the readout spot is projected on a photodiode", with adequate
  bandwidth.
- §12.3.1 and Figure 17: HFMI = (V₂ − V₃)/V₁, where V₁ is the reading on an
  unpitted area, V₂ the maximum between pits and V₃ the minimum on pits.
  Figure 18 (replaced by Amendment 2, same limits): HFMI limits against track
  pitch, at a spatial frequency of 802 ± 26 pits/mm.
- BBC RD 1986/01 §1.2: spatial cut-off `fc = 2N/λ`; §3.6: measured cut-off
  ≈ 1 250 cycles/mm, agreeing with the theory; §3.3: the main
  high-frequency loss is the aperture loss of the reading spot.
- Philips Technical Review 33 (1973) pp. 186–189 (Bouwhuis & Burgstede):
  Gaussian pupil filling using about 80 % of the laser power; half-intensity
  spot diameter ≈ 0.9 µm; maximum modulation when the pit-bottom light is
  180° out of phase with the land light.
- Isailović 1985 §5.4.2 (eqs 5-87, 5-88), §5.6 (Fig. 5-38) and §3.1
  ([Isailović 1985](secondary-sources.md#isailović-videodisc-and-optical-memory-systems-1985)): with one objective both illuminating and collecting,
  the response "falls gradually" to a cut-off at `2NA/λ`; the
  diffraction-limited transfer is the overlap of two displaced pupils; the
  cut-off at 55 mm is 13.2 MHz (NA 0.4, λ 0.63 µm, 30 Hz). §3.4, App. 3.2
  and §6.3.2: the MTF's unequal attenuation of the FM sidebands changes the
  demodulated signal, depending on radius in CAV.
- To confirm (not yet read; [secondary sources](secondary-sources.md)): H.H.
  Hopkins, JOSA 69 (1979) 4–24; Bouwhuis et al. 1985, chapter "Read-out of
  optical discs".

## Model and equations

### Pupil

With the pupil radius normalised to 1, the Gaussian intensity across the
pupil is `exp(−β·ρ²)`. §11.1.3 sets the rim-to-centre ratio to 0.23, so

```
β = −ln 0.23 = 1.4697,   pupil amplitude a(ρ) = exp(−β·ρ²/2) for ρ ≤ 1
```

Cross-check: the fraction of a Gaussian beam's power inside the pupil is
`1 − e^(−β) = 0.77`, which agrees with PTR's "about 80 % of the laser power".

Derived spot properties (numerical evaluation of the Hankel transform of
`a(ρ)`):

| Property | Value | Check |
|----------|-------|-------|
| Half-intensity spot diameter | 0.871 µm | PTR: ≈ 0.9 µm |
| First diffraction ring | 0.72 % of peak | §11.1.4: < 2 % (uniform pupil: 1.75 %) |

### Detection and transfer function

§11.2 projects the whole returned spot onto one photodiode, so the detector
integrates all light the objective collects. For this arrangement, the
component of the signal that is linear in the disc's reflectance pattern has
the transfer function

```
H(ν) = ∫ a(η) a(η − ν) dη / ∫ a(η)² dη
```

the normalised autocorrelation of the pupil amplitude, with ν the spatial
frequency normalised to NA/λ. Derivation: the zero-order light reaching pupil
point η came from illumination direction η (amplitude a(η)); the first-order
light diffracted by a grating of frequency ν into η came from direction η − ν
(amplitude a(η − ν)); the detector integrates their interference over the
pupil. H is zero for ν ≥ 2, so the cut-off is

```
fc = 2·NA/λ = 1 264.2 cycles/mm
```

matching BBC §1.2 and the ≈ 1 250 cycles/mm that BBC §3.6 measured. The
inverse transform of H, the line-spread function, is the spot's intensity
profile integrated across the track, so it is **non-negative**, and the edge
response (its integral) rises monotonically.

Temporal cut-off `fc·v` for the scanning speeds of S8:

| Case | v | Cut-off |
|------|---|---------|
| CAV, 55 mm | 10.36 m/s | 13.09 MHz |
| CAV, 145 mm | 27.30 m/s | 34.52 MHz |
| CLV | 10.1–11.4 m/s | 12.77–14.41 MHz |

Pre-emphasis overshoot can take the instantaneous carrier to about 12.2 MHz
(S4). On CLV discs and at the inner CAV radius that lies close to the
cut-off, so those excursions are strongly attenuated by the readout itself.
This is a prediction for Phase 6 to check, not an assumption.

### Linear readout model (initial model)

The pit pattern along the track is `p(x)` = 1 on a pit, 0 on land. Written
with the D7 edges (`σ_k` = +1 at a pit start, −1 at a pit end) and the
line-spread function LSF with edge response ESF:

```
i(τ) = 1 − m · Σ_k σ_k · ESF(x_s(τ) − x_k)
```

`i` is normalised so that unpitted land reads 1 (V₁ = 1). `m` is the
modulation contrast of the pits.

**Setting m from the HFMI.** HFMI is defined on a pit train at
V = 802 pits/mm. Its third harmonic (2 406 cycles/mm) is above fc, so only the
fundamental of the 50 % square wave survives. The fundamental's peak-to-peak
amplitude is `(4/π)·H(V)`, so

```
HFMI = m · (4/π) · H(802 pits/mm),   H(802 pits/mm) = 0.21317   (ν/fc = 0.634)
```

The implementation (`src/optical_readout/readout_transfer.cpp`) evaluates
`H` by integrating across the track analytically (an erf) and along it with
64-point Gauss–Legendre quadrature after the substitution `x = cos θ`. It
reproduces the uniform-pupil closed form to 10⁻¹², and gives 0.21317 here.

**Physical limit of the linear model.** Because the LSF is non-negative,
`i` lies in [1 − m, 1]. On long pits, which the outer CAV radii resolve
fully, `i` reaches 1 − m. Intensity cannot be negative, so `m ≤ 1`, which
limits the linear model to

```
HFMI ≤ (4/π) · 0.21317 = 0.27142
```

Figure 18's band at the example pitch of 1.667 µm is about 0.17–0.41, with
midpoint 0.29. The linear model can therefore reach only the lower 40 % or so
of the band, and not the midpoint. This is a known limitation: a phase pit does not act
like a dark mark. Its signal comes from interference between pit and land
light (PTR p. 186), which the phase-pit model of plan Task 10.1 represents.

**Decision for the ideal baseline.** `m = 1`, giving HFMI = 0.27142. This is
the largest modulation the linear model represents without unphysical
intensity, and it lies inside the Figure 18 band at every pitch from 1.4 to
2.0 µm. The project may set a lower `hfmi`; a value that implies `m > 1` is a
validation error for the linear model.

### Readout rate

H is exactly zero above fc, so `i(τ)` is band-limited to `fc·v`. The largest
speed in range is CAV at the 150 mm disc edge (28.25 m/s → 35.7 MHz). S11
evaluates `i` at

```
Fr = 80 MHz
```

whose Nyquist frequency (40 MHz) is above every case, so sampling `i`
introduces no aliasing. Fr is twice the 40 MHz capture rate, so S13 can
sample by exact decimation after its anti-alias filter.

### Rendering

Implemented in `src/optical_readout/` (`ReadoutLineSpread`,
`MakeReadoutEdgeResponse`, `EdgeSumRenderer`, `LinearOpticalReadout`).

**Line-spread function.** `LSF(u) = 2∫₀^fc H(ν)·cos(2πνu) dν`, computed with
ν = fc·(1 − t²), which makes H's (fc − ν)^(3/2) behaviour at the cut-off
smooth in t, and 32 panels of 16-point Gauss–Legendre quadrature. H is
evaluated once, at the 512 nodes.

**Why the kernel cannot simply be truncated.** H has a corner at ν = 0
(`H ≈ 1 − c·|ν|`, as for every circular pupil), so the LSF falls only as
1/u² and `1 − ESF` as 1/u: 2.2 × 10⁻³ at 10 µm and 1.1 × 10⁻³ at 20 µm.
Cutting it off at any practical width would leave errors far above 10⁻⁶.

**Far field from the pattern's mean.** Write the pit pattern as
`p = ½ + d`. Inside the record `d = ±½` is the limited FM carrier, which
has no baseband content (the limiter's output is Σ odd harmonics of φ, with
no constant term), so the slowly varying far tail of the LSF sees only its
mean ½, which it passes with gain H(0) = 1. Only `d` needs the near
kernel:

```
i(τ) = 1 − m·[½ + ∫ k(x_s(τ) − x)·d(x) dx]
k = LSF·w,  w = 1 for |u| ≤ 10 µm, raised cosine to 0 at 20 µm
```

(register `S11.kernel_window`). The implemented transfer function is then
exact at DC and equals the kernel's transform K elsewhere, which matches H
within **1.1 × 10⁻⁴** at every spatial frequency of 110 cycles/mm or more
(unit tests; 110 cycles/mm is 3 MHz at the outer CAV speed, below the FM
spectrum's lower edge). The error peaks near the cut-off, where the window
smears H's corner, and falls only slowly with a wider window (6 × 10⁻⁵ at
±25 µm, 2.5 × 10⁻⁵ at ±40 µm), and not at all with a smoother taper, so
±20 µm is used. A pit pattern with content below 110 cycles/mm, such as
long pits from a future slice level (S6), would see K's DC mass,
1 − 0.00305, instead of 1; this must be revisited when audio or EFM exists.

**Edges.** With E the edge response of k, tabulated at 5 nm on [−W, W]
with Hermite interpolation (register `S11.kernel_table_spacing`):

```
i(τ) = 1 − m·[½ + M·d_left + Σ_window Δ_j·E(x_s(τ) − x_j)]
```

d steps by Δ = ±1 at each D7 edge; the steps within ±W of the spot are
summed and older ones are folded into `d_left`, the value of d just before
the window; M = E(W) = 0.99695 is the kernel's mass. The table's
interpolation error is below 1e-10 per edge (unit test against direct
integration), so below 1e-8 for the ≤ 64 edges in a window.

**The record's ends** (register `S11.record_boundary`, declared). The
record begins at arc length 0 (τ = 0) and ends at the cutting position of
the last frame. Outside it nothing is known about the track, so the pattern
there is taken as its mean, d = 0: the start adds a step `p₀ − ½`, with
`p₀` the state the first edge leaves (1 if it ends a pit, as S7 gives for
φ = 0), and the end a step `½ − p_end`. Only samples within 20 µm (2 µs at
the inner radius) of either end see these steps.

**Precision.** Arc lengths are doubles from the start of the run; at the
end of an hour (40 km) their spacing is 7 pm, 0.6 ps of track time, which
is within S7's 4 ps edge-time budget.

**Cost.** About 64 edges per sample at the inner radius, at 80 MHz.

**Host independence.** H is computed with the portable exp, log, erf,
arccos, sine and cosine of `src/dsp/portable_math.cpp`, and the
Gauss–Legendre rules with portable cosine, so the tables, and the RF, are
the same on every host (HLD §2.3).

## Parameters

| Register row | Value |
|--------------|-------|
| `S11.wavelength` | 632.8 nm (fixed) |
| `S11.numerical_aperture` | 0.40 ± 0.01 |
| `S11.rim_intensity` | 0.23 ± 0.01 |
| `S11.hfmi` | 0.27142 (derived: m = 1) |
| `S11.hfmi_reference_frequency` | 802 ± 26 pits/mm |
| `S11.readout_rate` | 80 MHz (derived) |
| `S11.kernel_window` | LSF unchanged to ±10 µm, raised cosine to 0 at ±20 µm (declared) |
| `S11.kernel_table_spacing` | 5 nm (declared) |
| `S11.record_boundary` | pattern = its mean outside the record (declared) |

## Assumptions and approximations

- **Linear model.** Pit edges add linearly. Error: unknown until the
  phase-pit model exists; plan Task 10.1 quantifies it.
- **Pits as stripes across the track.** The model uses the one-dimensional
  slice `H(νₓ, 0)`, which treats each pit as infinitely wide. A pit narrower
  than the spot (PTR: 0.8 µm against 0.87 µm) has a different effective
  transfer function. For the linear model the two-dimensional transfer is
  radially symmetric, so a centred pit of width w has
  `H_w(ν) = ∫ H(√(ν² + η²))·w·sinc(wη) dη`. For w = 0.8 µm (unit test):
  `H_w(0) = 0.72`, so a long pit reads 72 % as deep as a stripe; with m
  set from the HFMI at 802 pits/mm, the shape `H_w/H` differs by +8.7 % at
  300 cycles/mm, +3.7 % at 700, −5.2 % at 900, −13 % at 1000 and −18.9 % at
  0.833·fc. Across the carrier's sync-to-white range at 55 mm
  (734–898 cycles/mm) the difference is within ±5 %. This is the largest
  known error of the ideal readout; the width is itself unsourced (S9), and
  the phase-pit model (plan Task 10.1) replaces both.
- **Kernel window and far field.** Error 1.1e-4 in the transfer function
  above 110 cycles/mm (Rendering).
- **Perfect focus, no tilt, no aberration.** The disc is at the Figure 1a
  design point. Focus and tilt are Phase 8 work.
- **Adjacent tracks blank.** No crosstalk at the ideal setting (Phase 9).
- **No noise** (Phase 9).
- The edge-response table's interpolation error is below 10⁻¹⁰ per edge,
  so below 10⁻⁸ in `i` (Rendering).
- **The record's ends** are padded with the pattern's mean (Rendering).

## Input/output contract

- **In:** D7. **Out:** D8 — `i(τ)` at Fr = 80 MHz, `τ = n/Fr` from the D1
  origin, normalised so land = 1.

## Project YAML block

`optical_readout`:

| Key | Type | Default | Range | IEC tolerance |
|-----|------|---------|-------|---------------|
| `numerical_aperture` | float | 0.40 | 0.30–0.60 | 0.39–0.41 (§11.1.2) |
| `rim_intensity` | float | 0.23 | 0.05–1.0 | 0.22–0.24 (§11.1.3) |
| `hfmi` | float | the largest value with m ≤ 1 (0.27142 at nominal optics) | > 0, and m ≤ 1 | inside the Figure 18 band at the configured track pitch |

The wavelength is fixed at 632.8 nm (§11.1.1 gives no tolerance).

## Ideal setting

IEC §11 reference optics, m = 1, adjacent tracks blank, no noise.

## Verification

`tests/unit/test_readout_transfer.cpp` and
`tests/unit/test_optical_readout.cpp`: the computed H matches the
uniform-pupil closed form; the kernel's transfer matches H within 1.1e-4
from 110 cycles/mm; the LSF is non-negative; the table matches direct
integration within 1e-10; constant-frequency pit trains at 500, 802 and
1000 pits/mm give the predicted fundamental, `(2/π)·m·H`, within 1.1e-4 and
the mean level 1 − m/2; at 802 pits/mm the peak-to-peak is the HFMI,
inside Figure 18; the output does not depend on block size; and the
finite-width bound above. The spot diameter and ring level were computed
for this note (Pupil).

## Diagnostic tap

`optical_readout`: D8 as `float64` at Fr with a descriptor.

## Open items

- **Resolved: the Gaussian pupil weighting applies once.** van de Nes,
  Braat & Pereira (2006) §2.3, eqs (4)–(7)
  ([secondary sources](secondary-sources.md#open-access-sources-from-the-wider-search))
  write the reflected pupil field from the incident pupil field `A` and
  integrate it over the exit pupil with no further weighting, so the beam
  profile enters the transfer function once, as this note's model does.
  Kondo (1993) models an optical head the same way. The structure and the
  `2NA/λ` cut-off are also confirmed by Isailović §5.4.2 and §5.6. The
  literal `g = f` in Braat, Dirksen & Janssen §6.2 would apply the weighting
  twice; it is read as covering the lens's own aperture and aberrations,
  since the Gaussian is the incoming beam's profile and not a property of
  the lens. Checks ([approximate figures](secondary-sources.md#approximate-figures-from-the-final-search)): the integration gives the
  analytic uniform-pupil value (0.2507), and Toshiba's calculated roll-off
  for a 1990 Hi-Vision videodisc agrees with the model within about 1–2 dB
  at the same normalised frequency. Hopkins (1979) would still be a welcome
  direct check. The
  choice mattered. At 55 mm on the 8.1 MHz carrier, the uncompensated chroma
  error at 3.58 MHz (narrow-band estimate below) is +1.2 dB with a uniform
  pupil, +2.7 dB with the Gaussian applied once (this note's model), and
  +4.6 dB with it applied on both paths. HFMI at 802 pits/mm is 0.250, 0.213
  and 0.168 respectively.
- **Effect on demodulated video (prediction for Phase 6).** With the FM
  narrow-band approximation of Isailović eqs 6-17 and 6-18, a component at
  video frequency `f_m` demodulates with gain `(a + b)/2`, where `a` and `b`
  are `H` at `f_c − f_m` and `f_c + f_m` relative to `H` at `f_c`. With this
  note's `H` at 55 mm and an 8.1 MHz carrier, the gain is +0.3 dB at 1 MHz,
  +1.0 dB at 2 MHz and +2.7 dB at 3.58 MHz (+4.8 dB at 3.58 MHz on a
  9.3 MHz carrier). At 145 mm it stays within −0.12 dB. So, without
  equalisation, chroma comes out larger at the inner radius, as Isailović
  App. 3.2 states. These are estimates, valid only for small modulation
  index. This effect is real and stays in the RF: the ideal disc's RF is
  test-point RF, and ld-decode compensates the MTF itself. The round trip
  therefore tests that compensation against this model; the S12 diagnostic
  equaliser separates the two when they disagree
  ([S12](s12-player-front-end.md)).
- The linear model cannot represent the upper part of the Figure 18 band
  (above). Resolved by the phase-pit model (plan Task 10.1).
