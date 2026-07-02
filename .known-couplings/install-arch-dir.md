# Install lib/<arch> subdir must match package.c's runtime lib search

The root `CMakeLists.txt` installs the runtime and compiler static libs into
`PONY_INSTALL_LIBDIR`, which is platform-dependent because ponyc's own runtime
lib search is: `lib/pony/<version>/lib/${PONY_ARCH}` on Unix, flat
`lib/pony/<version>/lib` on Windows. ponyc finds those libs at run time relative
to its own binary — `package.c` (~752-772) uses `..\lib` on Windows
(`PLATFORM_IS_WINDOWS`, no arch component) and `../lib/<arch>` on Unix, where
`<arch>` is `opt->link_arch` if `--link-arch` was passed, else the `PONY_ARCH`
compile define. That define comes from the same `PONY_ARCH` cache variable
(`CMakeLists.txt:527`, `add_compile_definitions(PONY_ARCH="${PONY_ARCH}")`) that
the Unix install destination uses. So the install layout and package.c's search
must agree per platform: flat on Windows, arch-subdir on Unix.

So one cache variable drives both the arch subdir libs are installed into and the
arch subdir ponyc looks in. Keep it that way. Hardcoding or renaming the arch
component in either place — the install DESTINATION or the compile define /
package.c derivation — silently breaks a `cmake --install`: the libs land in one
subdir and ponyc searches another, so linking a user program fails to find
`libponyrt.a`. This single-source coupling is the fix for #3898 (the arch chosen
at `configure` is carried into `install` instead of defaulting): `cmake --install`
reads the destination baked into `cmake_install.cmake` at generate time, so the
configure-time arch and the install-time arch can't disagree.

The cross-compile branch (`if(PONY_CROSS_LIBPONYRT)`) installs a runtime built for
another arch into the same `lib/pony/<version>/lib/${PONY_ARCH}`, found via
`--link-arch=<that-arch>` at link time — the same subdir contract.

No unit test exercises this; it is a runtime path-resolution failure. Run: build
and `cmake --install` to a prefix, then compile and run a program with the
installed ponyc (the install-tree shim check) — with a non-default `arch=` to
confirm the carry.
