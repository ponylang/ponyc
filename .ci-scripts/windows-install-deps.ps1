# Install the Windows test-time dependencies for the ponyc suite: the
# mingw-w64 LLVM family and the runtime dependencies lldb imports against.
# Only the ponyc Windows job needs this (it runs the test suite with
# `-Uselldb yes`); the pony-compiler and tools Windows jobs do not, and the
# libs-only maybe-build job does not.
#
# History:
# (1) 2026-05-03: lldb-22.1.4-2 ("llvm: emutls rebuild") crashed
#     in liblldb.dll with 0xC0000005. Pinned llvm-libs,
#     clang-libs, lldb to 22.1.4-1.
# (2) 2026-05-11: msys2 bumped unpinned dependencies (gcc-libs
#     16.1.0-2 -> -3, plus expat/mpdecimal/python pkgrel bumps).
#     The pinned lldb 22.1.4-1 then failed to load with
#     0xC0000139 STATUS_ENTRYPOINT_NOT_FOUND — the rebuilt deps
#     had shifted symbols out from under lldb. Extended the pin
#     to those deps.
#
# See Discussion #5007. Installed via `pacman -U` from explicit
# URLs so the initial `-Syuu` can't re-introduce a broken
# combination.
function msys() { C:\msys64\usr\bin\bash.exe @('-lc') + @Args }
msys ' '
msys 'pacman --noconfirm -Syuu'
msys 'pacman --noconfirm -Syuu'
msys 'pacman --noconfirm -S --needed base-devel'
msys 'pacman --noconfirm -U https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-llvm-libs-22.1.4-1-any.pkg.tar.zst https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-clang-libs-22.1.4-1-any.pkg.tar.zst https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-lldb-22.1.4-1-any.pkg.tar.zst https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-gcc-libs-16.1.0-2-any.pkg.tar.zst https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-expat-2.8.0-1-any.pkg.tar.zst https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-mpdecimal-4.0.1-2-any.pkg.tar.zst https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-python-3.14.4-2-any.pkg.tar.zst'
msys 'pacman --noconfirm -Scc'
