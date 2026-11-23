# ld-creator

An application for mastering PAL and NTSC LaserDiscs: it modulates CVBS video,
analogue audio and EFM digital audio into a LaserDisc RF signal.

📖 **[Read the documentation at decode-orc.github.io/ld-creator](https://decode-orc.github.io/ld-creator)**

**ld-creator** takes the composite video defined by the
[CVBS file format specification](https://github.com/decode-orc/cvbs-file-format-specification),
together with its associated audio and EFM streams, and produces the RF signal
a LaserDisc player's pickup would read from a disc mastered to IEC 60856 (PAL)
or IEC 60857 (NTSC).

Its purpose is to close a measurement loop:

```
videosynth ──CVBS/WAV/EFM──► ld-creator ──RF──► ld-decode ──CVBS/WAV/EFM──► compare with truth
```

[videosynth](https://github.com/decode-orc/videosynth) generates a signal
whose every sample is known; ld-creator turns it into RF;
[ld-decode](https://github.com/happycube/ld-decode) decodes that RF back. Any
difference between what went in and what came out is a measured error in the
decode chain.

> **Status:** the repository is scaffolded for development. The command-line
> entry point, build, tests and documentation pipeline are in place; RF
> modulation is not yet implemented.

---

## Quick start

```bash
git clone --recurse-submodules https://github.com/decode-orc/ld-creator
cd ld-creator
nix develop "path:$PWD" --command cmake -S . -B build -G Ninja
nix develop "path:$PWD" --command cmake --build build
nix develop "path:$PWD" --command ctest --test-dir build -L unit --output-on-failure
```

Full instructions — CMake options, the Nix package, the pinned sister tools and
the documentation build — are in [BUILDING.md](BUILDING.md).

---

## Repository layout

```
include/ld_creator/   Public headers, one directory per module
src/                  Library implementation and the CLI entry point
tests/                Unit and functional test suites (see TESTING.md)
scripts/              Round-trip runners and analysis utilities
docs/                 MkDocs sources for the published user documentation
docs-tech/design/     High-level design specification
docs-tech/user/       Feature notes and implementation plans
docs-tech/*-specification*/  Specification submodules (CVBS format, analogue video, LaserVision)
flake.nix             Nix build, dev shell, and pinned videosynth / ld-decode
```

---

## Documentation

- [docs/](docs/) — user documentation, published at
  [decode-orc.github.io/ld-creator](https://decode-orc.github.io/ld-creator).
- [docs-tech/design/high-level-design.md](docs-tech/design/high-level-design.md) —
  design specification.
- [BUILDING.md](BUILDING.md) — building, running and pinned tools.
- [TESTING.md](TESTING.md) — testing vision, strategy and repository rules.
- [AGENTS.md](AGENTS.md) — contribution and coding standards.

---

## Licence

GNU General Public License v3.0 or later. See [LICENSE](LICENSE).
