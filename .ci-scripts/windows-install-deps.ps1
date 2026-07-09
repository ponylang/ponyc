# Install the Windows test-time dependencies for the ponyc suite: the
# mingw-w64 LLVM family and the runtime dependencies lldb imports against.
#
# Every version below is pinned, and installed by URL rather than by name so the
# `-Syuu` can't pull a newer one. Twice now an unpinned bump has left lldb unable
# to load. See Discussion #5007.
function msys() { C:\msys64\usr\bin\bash.exe @('-lc') + @Args }
msys ' '
msys 'pacman --noconfirm -Syuu'
msys 'pacman --noconfirm -Syuu'
msys 'pacman --noconfirm -S --needed base-devel'
msys 'pacman --noconfirm -U https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-llvm-libs-22.1.4-1-any.pkg.tar.zst https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-clang-libs-22.1.4-1-any.pkg.tar.zst https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-lldb-22.1.4-1-any.pkg.tar.zst https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-gcc-libs-16.1.0-2-any.pkg.tar.zst https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-expat-2.8.0-1-any.pkg.tar.zst https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-mpdecimal-4.0.1-2-any.pkg.tar.zst https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-python-3.14.4-2-any.pkg.tar.zst'
msys 'pacman --noconfirm -Scc'
