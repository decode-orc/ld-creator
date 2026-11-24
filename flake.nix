{
  description = "ld-creator - LaserDisc RF modulator for CVBS, audio and EFM";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
    flake-utils.url = "github:numtide/flake-utils";

    # Sister applications used for round-trip verification. Each is pinned
    # through flake.lock; update deliberately with
    #   nix flake update videosynth
    #   nix flake update ld-decode
    #
    # Their nixpkgs inputs are intentionally NOT made to follow ours: each tool
    # is built exactly as its own repository builds and tests it, so a
    # round-trip result is never confounded by a different dependency set.
    # Only flake-utils (pure Nix library code) is shared.

    # Truth generator: produces the CVBS, audio and EFM inputs to ld-creator.
    videosynth = {
      url = "github:decode-orc/videosynth";
      inputs.flake-utils.follows = "flake-utils";
    };

    # Decoder under test: demodulates ld-creator's RF back to CVBS.
    ld-decode = {
      url = "github:happycube/ld-decode";
      inputs.flake-utils.follows = "flake-utils";
    };
  };

  outputs = { self, nixpkgs, flake-utils, videosynth, ld-decode }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        # Git metadata is unavailable inside the Nix build sandbox, so the
        # commit hash is passed in as a version override instead.
        versionString = self.shortRev or self.dirtyShortRev or "0.1.0";

        fs = pkgs.lib.fileset;

        # Explicit source sets. A "path:" flake reference copies the whole
        # working tree, including any local build/ directory, whose
        # CMakeCache.txt would then hijack the sandboxed configure. Listing
        # only what each derivation needs also keeps the package hash stable
        # across documentation-only edits.
        packageSource = fs.toSource {
          root = ./.;
          fileset = fs.unions [
            ./CMakeLists.txt
            ./include
            ./src
            ./tests
            ./.clang-format
            ./.clang-tidy
          ];
        };

        docsSource = fs.toSource {
          root = ./.;
          fileset = fs.unions [
            ./mkdocs.yml
            ./docs
          ];
        };

        videosynthPkg = videosynth.packages.${system}.default;
        ldDecodePkg = ld-decode.packages.${system}.default;

        # Python environment for the MkDocs documentation site. Uses the default
        # python3 so packages come from the binary cache: Hydra only builds the
        # default interpreter's package set, so pinning e.g. python312 forces
        # local source builds (and their test suites).
        mkdocsPythonEnv = pkgs.python3.withPackages (ps: [
          ps.mkdocs
          ps.mkdocs-material
          ps.mkdocs-awesome-nav
        ]);

        # Python for analysis and comparison scripts (scripts/). Kept separate
        # from the MkDocs environment so either can change independently.
        analysisPythonEnv = pkgs.python3.withPackages (ps: [
          ps.numpy
          ps.scipy
        ]);

        ld-creator-docs = pkgs.stdenv.mkDerivation {
          pname = "ld-creator-docs";
          version = versionString;
          src = docsSource;

          nativeBuildInputs = [ mkdocsPythonEnv ];

          buildPhase = ''
            mkdocs build --strict
          '';

          installPhase = ''
            mkdir -p $out
            cp -r site/* $out/
          '';

          meta = with pkgs.lib; {
            description = "ld-creator documentation site";
            homepage = "https://github.com/decode-orc/ld-creator";
            license = licenses.gpl3Plus;
            platforms = platforms.all;
          };
        };

        ld-creator = pkgs.stdenv.mkDerivation {
          pname = "ld-creator";
          version = versionString;
          src = packageSource;

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
          ];

          buildInputs = with pkgs; [
            spdlog
            sqlite
            yaml-cpp
            gtest
          ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DLD_CREATOR_BUILD_TESTS=ON"
            "-DPROJECT_VERSION_OVERRIDE=${versionString}"
            # The sandbox only exposes the stdenv's unwrapped clang-tidy, which
            # cannot resolve libc++ headers; clang-tidy is a dev/CI gate. The
            # formatter available there is an unrelated version, so the format
            # gate is a dev/CI check too.
            "-DLD_CREATOR_ENABLE_CLANG_TIDY=OFF"
            "-DLD_CREATOR_ENABLE_CLANG_FORMAT=OFF"
          ];

          doCheck = true;
          # Keep the sandboxed check to the fast, mocked unit lane (AGENTS.md
          # §3.1). Functional and round-trip suites run in the dev shell or a
          # dedicated CI job.
          checkPhase = ''
            ctest -L unit --output-on-failure
          '';

          meta = with pkgs.lib; {
            description = "LaserDisc RF modulator for CVBS, audio and EFM";
            homepage = "https://github.com/decode-orc/ld-creator";
            license = licenses.gpl3Plus;
            mainProgram = "ld-creator";
            platforms = platforms.unix;
          };
        };
      in {
        packages = {
          default = ld-creator;
          inherit ld-creator;
          docs = ld-creator-docs;
          # Re-exported so the pinned sister tools can be built or run from this
          # repository without knowing their flake references.
          videosynth = videosynthPkg;
          ld-decode = ldDecodePkg;
        };

        apps = {
          default = flake-utils.lib.mkApp { drv = ld-creator; };
          ld-creator = flake-utils.lib.mkApp { drv = ld-creator; };
          videosynth = flake-utils.lib.mkApp {
            drv = videosynthPkg;
            exePath = "/bin/videosynth";
          };
          ld-decode = flake-utils.lib.mkApp {
            drv = ldDecodePkg;
            exePath = "/bin/ld-decode";
          };
        };

        checks.default = self.packages.${system}.default;

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            # Build toolchain
            cmake
            ninja
            pkg-config
            ccache
            clang-tools

            # Libraries
            spdlog
            sqlite
            yaml-cpp
            gtest

            # Scripts and documentation
            analysisPythonEnv
            mkdocsPythonEnv

            # Pinned sister tools for round-trip verification
            videosynthPkg
            ldDecodePkg
          ];
        };

        formatter = pkgs.nixpkgs-fmt;
      });
}
