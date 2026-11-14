{ pkgs, common, libs, src }:
let
  version = pkgs.lib.removeSuffix "\n" (builtins.readFile (src + "/VERSION"));
  compiler = common.stdenv.cc;
  gccDirectory = "${compiler.cc}/lib/gcc/${common.stdenv.hostPlatform.config}/${compiler.cc.version}";
  includeDirectories = [
    "${pkgs.lib.getDev compiler.libc}/include"
    "${gccDirectory}/include"
    "${gccDirectory}/include-fixed"
    "${pkgs.lib.getDev pkgs.openssl}/include"
    "${pkgs.lib.getDev pkgs.zlib}/include"
  ];
  libraryDirectories = [
    "${pkgs.lib.getLib compiler.libc}/lib"
    "${compiler.cc}/lib"
    gccDirectory
    "${pkgs.lib.getLib compiler.cc}/lib"
    "${pkgs.lib.getLib pkgs.openssl}/lib"
    "${pkgs.lib.getLib pkgs.zlib}/lib"
  ];
  toolchain = pkgs.writeText "ponyc-toolchain.json" (builtins.toJSON {
    inherit version includeDirectories libraryDirectories;
    opensslVersion = pkgs.openssl.version;
    compiler = toString compiler;
    ar = "${compiler.bintools}/bin/ar";
  });
in common.stdenv.mkDerivation {
  pname = "ponyc";
  inherit version src;
  inherit (common) nativeBuildInputs buildInputs;
  dontUseCmakeConfigure = true;
  dontStrip = true;
  enableParallelBuilding = true;
  doCheck = true;

  configurePhase = ''
    runHook preConfigure
    mkdir -p build lib/llvm/src/compiler-rt/lib
    ln -s ${libs} build/libs
    ln -s ${libs}/share/pony/compiler-rt-builtins \
      lib/llvm/src/compiler-rt/lib/builtins
    cmake --preset x86-64-release \
      -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
      -DCMAKE_INSTALL_PREFIX="$out"
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    cmake --build --preset x86-64-release --parallel "$NIX_BUILD_CORES"
    runHook postBuild
  '';

  checkPhase = ''
    runHook preCheck
    set -o pipefail
    export LOCALE_ARCHIVE=${common.localeArchive}
    for setting in ''${!GTEST_@}; do unset "$setting"; done
    for suite in libponyc libponyrt; do
      timeout 1800 ctest --preset x86-64-release -V -R "^$suite[.]tests$" \
        --no-tests=error --output-on-failure 2>&1 | tee "$TMPDIR/$suite.log"
      python3 -B - "$TMPDIR/$suite.log" <<'PY'
    from pathlib import Path
    import re
    import sys

    output = Path(sys.argv[1]).read_text()
    patterns = [
        r"\[==========\] Running ([1-9][0-9]*) tests? from ",
        r"\[==========\] ([1-9][0-9]*) tests? from .* ran\.",
        r"\[  PASSED  \] ([1-9][0-9]*) tests?\.",
    ]
    counts = [re.findall(pattern, output) for pattern in patterns]
    if not all(len(count) == 1 for count in counts):
        sys.exit(f"{sys.argv[1]}: missing nonzero GTest completion counts")
    listed, ran, passed = (int(count[0]) for count in counts)
    skipped = re.findall(r"\[  SKIPPED \] (\S+) tests?,", output)
    if len(skipped) > 1 or (skipped and not skipped[0].isdigit()):
        sys.exit(f"{sys.argv[1]}: invalid GTest skipped summary")
    skipped = int(skipped[0]) if skipped else 0
    if listed != ran or passed + skipped != ran:
        sys.exit(f"{sys.argv[1]}: GTest completion counts disagree")
    PY
    done
    runHook postCheck
  '';

  installPhase = ''
    runHook preInstall
    cmake --install build/build_x86-64-release --config release
    runHook postInstall
  '';

  postFixup = ''
    versioned="$out/lib/pony/${version}"
    resource_include=$(find "$versioned" -path '*/clang/*/include' -type d)
    test -n "$resource_include"
    test "$(printf '%s\n' "$resource_include" | wc -l)" -eq 1
    mkdir -p "$out/nix-support"
    python3 -B - ${toolchain} "$out" "$resource_include" <<'PY'
    from pathlib import Path
    import json
    import sys

    metadata = json.loads(Path(sys.argv[1]).read_text())
    cache = Path("build/build_x86-64-release/CMakeCache.txt").read_text()
    selection = [line.removeprefix("PONY_SSL_FLAG:INTERNAL=")
                 for line in cache.splitlines()
                 if line.startswith("PONY_SSL_FLAG:INTERNAL=")]
    if len(selection) != 1:
        sys.exit("missing unique CMake SSL definition")
    metadata["cmakeSslDefine"] = selection[0]
    metadata["interpreter"] = (Path(metadata["compiler"])
                               / "nix-support/dynamic-linker").read_text().strip()
    metadata["includeDirectories"].append(sys.argv[3])
    (Path(sys.argv[2]) / "nix-support/installed-toolchain.json").write_text(
        json.dumps(metadata, indent=2) + "\n")
    PY
    includes=${pkgs.lib.escapeShellArg (pkgs.lib.concatMapStringsSep " " (directory: "-isystem ${directory}") includeDirectories)}
    libraries=${pkgs.lib.escapeShellArg (pkgs.lib.concatMapStringsSep " " (directory: "-L${directory}") libraryDirectories)}
    for tool in ponyc pony-lsp pony-lint pony-doc pony-dep pony-agent-server; do
      mv "$versioned/bin/$tool" "$versioned/bin/.$tool-real"
      cat > "$versioned/bin/$tool" <<WRAPPER
    #!${pkgs.runtimeShell}
    export NIX_CC=${compiler}
    export NIX_CFLAGS_COMPILE="$includes -isystem $resource_include\''${NIX_CFLAGS_COMPILE:+ \$NIX_CFLAGS_COMPILE}"
    export NIX_LDFLAGS="$libraries\''${NIX_LDFLAGS:+ \$NIX_LDFLAGS}"
    exec "$versioned/bin/.$tool-real" "\$@"
    WRAPPER
      chmod +x "$versioned/bin/$tool"
    done
  '';

  passthru = { inherit libs; };
  meta = {
    description = "Pony compiler and language tools";
    homepage = "https://www.ponylang.io/";
    license = pkgs.lib.licenses.bsd2;
    platforms = [ "x86_64-linux" ];
    mainProgram = "ponyc";
  };
}
