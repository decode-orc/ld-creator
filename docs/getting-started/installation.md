# Installation

ld-creator is built from source with the Nix development shell. There are no binary releases yet.

## Requirements

- [Nix](https://nixos.org/){target="_blank"} with flakes enabled.
- Git.

## Get the source

```bash
git clone --recurse-submodules https://github.com/decode-orc/ld-creator
cd ld-creator
```

The submodules provide the specification documents used by developers. If you cloned without them:

```bash
git submodule update --init --recursive
```

## Build

Builds are out of source — CMake will refuse to configure into the source directory. Use a separate build directory such as `build/`:

```bash
nix develop "git+file://$PWD" --command cmake -S . -B build -G Ninja
nix develop "git+file://$PWD" --command cmake --build build
```

The program is written to `build/ld-creator`. Check it runs:

```bash
nix develop "git+file://$PWD" --command ./build/ld-creator --version
```

## Run without building a checkout

Nix can build and run ld-creator directly:

```bash
nix run github:decode-orc/ld-creator -- --help
```

## Sister tools

The development shell also provides the exact versions of **videosynth** and **ld-decode** that ld-creator is tested against, so you do not need to install them separately:

```bash
nix develop "git+file://$PWD" --command videosynth --help
nix develop "git+file://$PWD" --command ld-decode --help
```

!!! note
    The first `nix develop` builds or downloads both sister tools, which can take some time.
