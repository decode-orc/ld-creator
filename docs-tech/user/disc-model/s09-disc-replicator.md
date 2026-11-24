# S9 — Disc replicator (`disc_replicator`)

## Physical process

The developed glass master is electroplated to make a metal master; further
electroforming generations lead to the stamper, which moulds the pits into a
transparent substrate. The substrate is coated with a reflective layer, and
two substrates are bonded information side to information side. The player
reads through the transparent substrate, so it sees each pit as a relief in
the reflective layer.

## Sources

- IEC 60857-1986 Introduction: a transparent substrate carries the
  information under a reflective coating; two substrates are assembled
  information surface to information surface; readout is through the
  substrate.
- Amendment 1 Figure 21: material build-up (substrate, information pits,
  reflective layer, glue and protective layer).
- §5.1 and Figure 1a: substrate refractive index and thickness; the dotted
  line is the nominal disc (n = 1.525, protective layer 1.225 mm) for which
  the player's objective is designed.
- §5.3: reflectivity ≥ 70 % (double pass, unmodulated area).
- §12.1 (RMI, RRI, Figures 15 and 16) and §12.3 (HFMI, Figure 18): the
  measured signals that a finished disc must give.
- BBC RD 1986/01 §2.4: master electroplated, metal master separated and used
  to press copies. §3.7: packing density is limited by the reading optics,
  "not by the mastering or pressing process".
- Philips Technical Review 33 (1973) pp. 178–179: moulds made "by an
  electroplating process"; pressing surface roughness ≤ 0.01 µm; pits 0.8 µm
  wide and 0.16 µm deep in the VLP prototype
  ([secondary sources](secondary-sources.md)).
- Isailović 1985 ([Isailović 1985](secondary-sources.md#isailović-videodisc-and-optical-memory-systems-1985)): §2.5, "ideally, each replication step
  will be an exact reproduction" of the master; §2.3.1.3, optimum pit depth
  `λ/(4n)`; §6.2, departures (edge blur by metallization, bump shape) and
  dropout statistics.

## Model and equations

At the ideal setting the pressed disc is an **exact copy** of the cut pit
map: every edge in D5 appears unchanged in D6.

The justification is BBC §3.7: the measured resolution of real pressed discs
was limited by the readout optics, not by mastering or pressing. That is
evidence that replication error is small against the readout loss modelled
in S11, though not a bound on it.

Pit depth, width and wall slope are not represented at the ideal setting.
In S11's linear model their whole effect on the signal is the modulation
contrast, which is set by the HFMI (S11). Representing them explicitly is
Phase 8 work (plan Task 8.3).

**Disc defects.** Physical dropouts (inclusions, scratches, pin-holes in the
reflective layer) are disc defects and belong to this stage or S11 as an
RF-domain impairment, so that a decoder's RF dropout detection can be
measured (decision recorded in HLD §1.1). They are later work.

## Parameters

| Register row | Value |
|--------------|-------|
| `S9.substrate_index` | 1.525 (Figure 1a nominal) |
| `S9.substrate_thickness` | 1.225 mm (Figure 1a nominal) |
| `S9.reflectivity` | ≥ 70 % (§5.3) |

None of these is a project key in the ideal scope. The substrate values are
the design point of the IEC §11 reference objective, so they produce no
aberration; they become keys only when focus and aberration effects are
modelled (plan Task 8.4).

## Assumptions and approximations

- Exact copy: no geometric change, no defects. Error bound: none available
  from a source; recorded as unsourced, ideal.

## Input/output contract

- **In:** D5. **Out:** D6, equal to D5 at the ideal setting.

## Project YAML block

None in the ideal scope. The block name `disc_replicator` is reserved for
Phase 8 keys.

## Ideal setting

Exact copy.

## Verification

Identity unit test (`tests/unit/test_disc_replicator.cpp`): edges and
watermark pass unchanged. Implemented as `ExactCopyReplicator`
(`src/disc_replicator/`).

## Diagnostic tap

None at the ideal setting.

## Open items

- How faithfully replication copies pit geometry. The mechanisms are now
  sourced (Isailović §6.2.5): the reflective layer blurs pit edges like a
  low-pass filter, unequal blurring of the two edges gives jitter, and the
  moulded bump varies "from almost Gaussian to T shapes". No magnitudes are
  given. Approximate values ([approximate figures](secondary-sources.md#approximate-figures-from-the-final-search)): moulding
  transfers 72–96 % of stamper depth (CD, Hougum 1996), poorer at the outer
  radius; 2p lacquer shrinks at most about 15 % by volume (PTR 40, 1982), so
  at most about 6–18 nm of a 0.12 µm pit. Electroforming: no figure.
- Pit width and wall slope for the pressed LaserVision disc. Depth: the
  readout optimum is `λ/(4n)` = 103.7 nm at the Figure 1a design point
  (Isailović §2.3.1.3; register row `S9.pit_depth_optimum`), and the master
  resist is 1100–1500 Å thick. Production figures
  ([secondary sources](secondary-sources.md#open-access-sources-from-the-wider-search)):
  0.12 µm deep in PTR 39 (1980) and PTR 40 (1982); widths 0.6 µm (PTR 39;
  DiscoVision stamper bumps, US 4,500,392) and 0.4 µm (PTR 40 and 42, with
  an SEM of a production disc). The VLP prototype was 0.8 µm wide (PTR 33).
  The widths still conflict, so no width is adopted. Philips's design
  values (US 4,230,915): wall angle 45–50° from the normal (acceptable
  30–65°), and width decreasing with radius (example 800 nm inner, 500 nm
  outer). With 45° walls the land-plane and half-depth widths differ by
  0.12 µm, which may explain the conflict ([approximate figures](secondary-sources.md#approximate-figures-from-the-final-search)).
  No measurement of a LaserVision pit wall has been found.
- Replication process. Philips's European discs were made by the 2p
  photopolymerisation process (PTR 40, 1982); DiscoVision's by moulding
  from nickel stampers (US 4,500,392). The model does not yet distinguish
  them.
- Dropout statistics. Isailović §6.2.2, §6.5.2 and §6.6 give their shape:
  lengths of one to two pits from mastering and up to about six from plastic
  inclusions; count against duration fitted by `a·e^(−bx)`, `a/x²` or
  `a·e^(−b√x)`; rate ∝ radius in CAV for a uniform defect density. They give
  no rate that applies to a typical disc ("several orders" of variation), so
  the rate is a project choice when dropouts are added. §12.2.1 bounds the
  length at 100 µs. A reference point for that choice: about 15 dropouts per
  frame as a background rate and about 50 per frame as "borderline" for a
  repaired poor stamper (US 4,500,392).
