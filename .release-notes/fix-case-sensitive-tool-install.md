## Fix install of ponyc-compiled tools on case-sensitive filesystems

Installing ponyc failed during `cmake --install` on case-sensitive filesystems (most Linux setups) because the ponyc-compiled tools (`pony-lsp`, `pony-lint`, `pony-doc`) were written to a directory named after the CMake configuration verbatim (`build/Release`), while `install()` reads them from the ponyc binary's lowercase per-config directory (`build/release`):

```
CMake Error at build/build_release/cmake_install.cmake (file):
  file INSTALL cannot find ".../build/release/pony-lsp": No such file or directory.
```

Case-insensitive filesystems (macOS, Windows) treated the two paths as the same directory, so the mismatch only surfaced where the filesystem is case-sensitive. The tools now build into the same lowercase per-config directory `install()` reads, so the install succeeds everywhere.
