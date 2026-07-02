# use= option validation (cmake/PonyUses.cmake) ↔ BSD reject scripts, the Windows allowlist, FreeBSD smokes, the wrappers, BUILD.md

The `use=`/`-Use` build-option set, and the per-platform rules that reject some of
them, live in one place: **`cmake/PonyUses.cmake`** (include()d from
`CMakeLists.txt` between `project()` and `find_package(LLVM ...)`). It parses the
comma- or whitespace-separated `PONY_USES` string, rejects unknown options and
platform-unsupported ones, and sets the `PONY_USE_*` variables the `CMakeLists.txt`
blocks consume. Several things elsewhere depend on its exact behavior, and none of
them are exercised by a build near the module:

1. **BSD reject scripts** — `.ci-scripts/openbsd-reject-unsupported-builds.sh`
   and `.ci-scripts/dragonfly-reject-unsupported-builds.sh` run
   `gmake configure use=<unsupported>` on a checkout with **no libs built** and
   `grep -q "not supported on OpenBSD"` / `"not supported on DragonFly"`. The
   constraints:
   - **Placement.** The include stays between `project()` and
     `find_package(LLVM REQUIRED)`. Below `project()` the OS checks read an unset
     `CMAKE_HOST_SYSTEM_NAME` and silently stop rejecting; below `find_package` a
     bad `use=` fails with "LLVM not found" instead of the platform message.
   - **Needs a base compiler.** Validation now runs during cmake configure, so
     `project()`'s compiler detection must succeed before the rejection fires —
     unlike the old make-parse-time `$(error)`, which needed no compiler. The BSD
     VMs have a working base `cc`/`c++` (they build ponyc), so this holds; the
     DragonFly reject step uses base `cc` (not the gcc13 the build uses), which
     still configures a trivial `project()` probe.
   - **Message shape.** Each rejection message must contain the exact substring
     `not supported on <OS>`, and that phrase must fall within the first ~78
     columns of the message — CMake's `message()` reflows at ~78 columns, so a
     reword that pushes the phrase past the wrap splits it across lines and the
     `grep -q` fails. (The longest, "UndefinedBehaviorSanitizer is not supported
     on DragonFly", is 56 columns — well within the wrap.)
   - **dtrace ordering.** For `dtrace`, the OS rejection must precede the
     `find_program(dtrace)` tool check, or on a BSD without the tool the message
     becomes "No dtrace compatible ..." and the grep fails.
   - The reject scripts enumerate the rejected options (OpenBSD: valgrind,
     thread/address/undefined_behavior sanitizers, coverage, dtrace; DragonFly:
     the three sanitizers + dtrace). Add or drop a platform rejection here and the
     matching script's loop must change too.

2. **Windows allowlist.** On MSVC the module rejects every option not in
   `_pony_windows_uses` (`systematic_testing`, `pool_retain`) — the only two that
   compile and link on Windows, verified by building each on Windows/MSVC. The
   rest need POSIX (`pool_memalign`'s `posix_memalign`, `runtime_tracing`'s
   `fnmatch.h`, `scheduler_scaling_pthreads`' pthreads) or Clang/GCC toolchain
   features `cl.exe` doesn't provide (the sanitizers' `-fsanitize=`), or don't
   compile clean (`pooltrack`, `runtimestats`); `coverage` "builds" but is a
   silent no-op on MSVC, so it is not allowed. This needs `MSVC`, which
   `project()` sets, so it depends on the placement in (1). To make a rejected
   option work on Windows: fix its runtime C, then add it to `_pony_windows_uses`
   and add a Windows reject/smoke test — there is none today; no Windows CI job
   passes a `use=` option. (Windows presets, when added, use the Visual Studio
   multi-config generator with `binaryDir` `build/build`, not `build/build_<cfg>`.)

3. **FreeBSD smoke scripts** — `.ci-scripts/freebsd-valgrind-smoke.sh`,
   `freebsd-coverage-smoke.sh`, `freebsd-sanitizer-smoke.sh` run
   `gmake configure use=<opt>` and expect it to **succeed** on FreeBSD. The module
   keys rejections on OpenBSD/DragonFly only, so FreeBSD passes; add a FreeBSD
   rejection for one of those options and the smoke breaks.

4. **The wrappers** — `Makefile` and `make.ps1` forward `use=` / `-Use` to CMake
   as `-DPONY_USES="..."` and no longer know the option set. They only guard that
   `use=` is given to `configure`. New options need no wrapper change.

5. **BUILD.md** — documents the option list and the OpenBSD/DragonFly
   unsupported-use sections. Keep it in sync when the set or the rules change.

6. **Consumption blocks** — `CMakeLists.txt` (and `src/libponyrt/CMakeLists.txt`
   for `PONY_USE_DTRACE`) read the `PONY_USE_*` variables. A new option needs an
   entry in the module *and* a consumption block, or it validates but does nothing.

Run: the tier-3 BSD legs exercise the reject and smoke scripts
(`.github/workflows/ponyc-tier3.yml`: freebsd, openbsd, dragonflybsd);
`ponyc-weekly-checks.yml` (`use_directives`, `with_sanitizers`) exercises the
options on Linux/macOS.
