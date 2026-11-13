{ pkgs }:
{
  stdenv = pkgs.gccStdenv;
  localeArchive = "${pkgs.glibcLocales}/lib/locale/locale-archive";
  nativeBuildInputs = [
    pkgs.cmake
    pkgs.ninja
    pkgs.clang
    pkgs.gcc
    pkgs.python3
    pkgs.pkg-config
    pkgs.coreutils
    pkgs.findutils
    pkgs.binutils
    pkgs.git
    pkgs.file
  ];
  buildInputs = [ pkgs.zlib pkgs.openssl pkgs.libxml2 pkgs.ncurses ];
}
