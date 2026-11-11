## Add NixOS linking support

ponyc's embedded linker can now link Pony programs on NixOS when run inside `nix-shell` or `nix develop`. Previously, linking failed because NixOS places libc, GCC runtime libraries, and system headers under `/nix/store` instead of the standard `/usr/lib` and `/usr/include` paths that ponyc searches.

When the `NIX_CC` environment variable points to a Nix compiler wrapper with a `nix-support/` directory, ponyc reads the dynamic linker path from `nix-support/dynamic-linker`, extracts library directories from `NIX_LDFLAGS`, reads the glibc prefix from `nix-support/orig-libc` (for CRT startup objects), reads compiler runtime library paths from `nix-support/cc-ldflags` (for libgcc/libgcc_s), and extracts system include directories from `NIX_CFLAGS_COMPILE`. These paths are used for CRT object discovery, GCC runtime discovery, the ELF dynamic linker (PT_INTERP), `-L`/`-rpath` link flags, and C shim include paths. On macOS with nix-darwin, only library search paths are added.

If ponyc detects it is running on NixOS without the Nix environment variables set, it prints a diagnostic suggesting `nix-shell` or `nix develop`.
