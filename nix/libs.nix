{ pkgs, common, sources, src }:
common.stdenv.mkDerivation {
  pname = "ponyc-libs";
  version = pkgs.lib.removeSuffix "\n" (builtins.readFile (src + "/VERSION"));
  inherit src;
  inherit (common) nativeBuildInputs buildInputs;
  dontUseCmakeConfigure = true;
  dontStrip = true;
  dontMoveLib64 = true;
  enableParallelBuilding = true;

  postUnpack = ''
    mkdir -p "$sourceRoot/lib/llvm"
    cp -R ${sources.llvm} "$sourceRoot/lib/llvm/src"
    chmod -R u+w "$sourceRoot/lib/llvm/src"
    find "$sourceRoot/lib/llvm/src" -name .git -type f -delete
  '';

  configurePhase = ''
    runHook preConfigure
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    cmake -DJOBS="$NIX_BUILD_CORES" \
      -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
      -DCMAKE_INSTALL_PREFIX="$out" \
      -DPONYC_GBENCHMARK_URL=${sources.benchmark} \
      -DPONYC_GOOGLETEST_URL=${sources.googletest} \
      -P lib/build-libs.cmake
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p "$out/share/pony"
    cp -R lib/llvm/src/compiler-rt/lib/builtins \
      "$out/share/pony/compiler-rt-builtins"
    runHook postInstall
  '';

  passthru.llvmRevision = sources.llvmRevision;
}
