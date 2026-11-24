# Building ld-creator

The Nix flake is the authoritative build environment. Its development shell
supplies CMake, Ninja, a C++20 toolchain, spdlog, SQLite, yaml-cpp, Google Test,
clang-format/clang-tidy, Python 3 (NumPy, SciPy), MkDocs — and the pinned
sister tools `videosynth` and `ld-decode`.

## Requirements

- [Nix](https://nixos.org/) with flakes enabled.
- Git, to clone the repository and its specification submodules.

```bash
git clone --recurse-submodules https://github.com/decode-orc/ld-creator
cd ld-creator
```

The submodules provide the specification documents under
[docs-tech/](docs-tech/). If you already cloned without them:

```bash
git submodule update --init --recursive
```

## Configure and build

Builds are **out of source**: CMake refuses to configure into the source
directory. The conventional build directory is `build/` (git-ignored); any
other directory, inside or outside the checkout, works too.

```bash
nix develop "path:$PWD" --command cmake -S . -B build -G Ninja
nix develop "path:$PWD" --command cmake --build build
```

The binary lands in `build/ld-creator`.

RF modulation is compute bound, so a build with no `CMAKE_BUILD_TYPE` set
defaults to `Release` rather than to an unoptimised build; the chosen type is
printed at configure time. Override it for debugging work:

```bash
nix develop "path:$PWD" --command cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

Multi-config generators (Ninja Multi-Config, Xcode) are left alone — they
select the configuration at build time.

### CMake options

| Option | Default | Effect |
|--------|---------|--------|
| `-DCMAKE_BUILD_TYPE=<type>` | `Release` | `Debug`, `Release`, `RelWithDebInfo` or `MinSizeRel` |
| `-DLD_CREATOR_BUILD_TESTS=OFF` | `ON` | Skip the test targets and the Google Test dependency |
| `-DLD_CREATOR_ENABLE_CLANG_FORMAT=OFF` | `ON` | Skip the clang-format check during the build |
| `-DLD_CREATOR_ENABLE_CLANG_TIDY=OFF` | `ON` | Skip clang-tidy static analysis during the build |
| `-DPROJECT_VERSION_OVERRIDE=<v>` | git short hash | Version string reported by `--version` |

## Tests

```bash
# Build every test binary
nix develop "path:$PWD" --command cmake --build build --target ld_creator_tests

# Fast, mocked, hermetic lane
nix develop "path:$PWD" --command ctest --test-dir build -L unit --output-on-failure

# Filesystem, executable and round-trip tests
nix develop "path:$PWD" --command ctest --test-dir build -L functional --output-on-failure
```

See [TESTING.md](TESTING.md) for the testing strategy and conventions.

## Nix package and apps

```bash
nix build "path:$PWD"                 # ld-creator package (runs the unit lane) → ./result
nix run "path:$PWD" -- --help         # run ld-creator
nix flake check "path:$PWD"           # build and test the package
```

## Pinned sister tools

`flake.nix` pins the applications ld-creator works with; exact revisions are
recorded in `flake.lock`.

| Input | Repository | Role |
|-------|------------|------|
| `videosynth` | [decode-orc/videosynth](https://github.com/decode-orc/videosynth) | Generates the truth CVBS, audio and EFM that ld-creator modulates |
| `ld-decode` | [happycube/ld-decode](https://github.com/happycube/ld-decode) | Decodes ld-creator's RF; the system under test |

Each tool builds with its **own** pinned nixpkgs, exactly as its upstream
repository builds it, so a round-trip result is never confounded by a different
dependency set.

Both are on `PATH` inside the dev shell, and are also exposed as flake outputs:

```bash
nix develop "path:$PWD" --command videosynth --version
nix develop "path:$PWD" --command ld-decode --help

nix run "path:$PWD#videosynth" -- --help
nix run "path:$PWD#ld-decode" -- --help
nix build "path:$PWD#videosynth"      # or #ld-decode
```

To move a pin (a deliberate change that can shift every round-trip result —
commit it on its own):

```bash
nix flake update videosynth --flake "path:$PWD"
nix flake update ld-decode --flake "path:$PWD"
```

The specification repositories are documents rather than applications and are
pinned as git submodules under `docs-tech/` (see below).

## Documentation

The user documentation in [docs/](docs/) is built with MkDocs Material:

```bash
nix develop "path:$PWD" --command mkdocs serve           # http://127.0.0.1:8000
nix develop "path:$PWD" --command mkdocs build --strict  # static site in ./site
nix build "path:$PWD#docs"                               # static site in ./result
```

Technical documentation lives in [docs-tech/](docs-tech/) and is read in the
repository:

| Path | Contents |
|------|----------|
| [docs-tech/design/high-level-design.md](docs-tech/design/high-level-design.md) | Living design specification |
| [docs-tech/user/](docs-tech/user/) | Feature notes and implementation plans |
| [docs-tech/cvbs-file-format-specification/](docs-tech/cvbs-file-format-specification/) | CVBS file format (submodule, read-only) |
| [docs-tech/analogue-video-specifications/](docs-tech/analogue-video-specifications/) | IEC 60856/60857 LaserVision, EFM and analogue video standards (submodule, read-only) |
