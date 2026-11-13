{
  description = "Pony compiler and development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      common = import ./nix/common.nix { inherit pkgs; };
      libsSource = builtins.path {
        path = self;
        name = "ponyc-libs-source";
        filter = path: type:
          let relative = pkgs.lib.removePrefix "${self}/" (toString path);
          in toString path == toString self
            || relative == "VERSION"
            || relative == "lib"
            || builtins.elem relative [
              "lib/CMakeLists.txt"
              "lib/CMakePresets.json"
              "lib/build-libs.cmake"
              "lib/ponylto_stub.cc"
              "lib/llvm"
              "lib/llvm/patches"
            ]
            || pkgs.lib.hasPrefix "lib/llvm/patches/" relative;
      };
      sources = import ./nix/sources.nix { inherit pkgs libsSource; };
      libs = import ./nix/libs.nix {
        inherit pkgs common sources;
        src = libsSource;
      };
      ponyc = import ./nix/package.nix {
        inherit pkgs common libs;
        src = self;
      };
    in {
      devShells.${system}.default = pkgs.mkShell.override {
        stdenv = common.stdenv;
      } {
        inherit (common) nativeBuildInputs buildInputs;
        LOCALE_ARCHIVE = common.localeArchive;
      };
      packages.${system} = { inherit ponyc; default = ponyc; };
      checks.${system}.installed-toolchain = pkgs.runCommand
        "ponyc-installed-toolchain" {
          nativeBuildInputs = [ pkgs.python3Minimal pkgs.binutils ];
        } ''
          set -o pipefail
          python3 -B ${./.ci-scripts/nix/nix-installed-checks.py} \
            --prefix ${ponyc} --workdir "$TMPDIR/installed-checks" \
            | tee "$TMPDIR/installed-checks.log"
          grep -Fx 'Installed toolchain checks passed' \
            "$TMPDIR/installed-checks.log"
          touch "$out"
        '';
    };
}
