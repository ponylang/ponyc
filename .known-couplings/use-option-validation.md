# use= option validation (`cmake/PonyUses.cmake`) couples to the BSD reject scripts

`cmake/PonyUses.cmake` parses the `PONY_USES` string, rejects unknown and
platform-unsupported options, and sets the `PONY_USE_*` variables the build consumes.
A few things outside it depend on its exact behavior, and none are exercised by a build
near the module. (`BUILD.md` documents the user-facing option list and the
OpenBSD/DragonFly unsupported sections — keep it in sync, but that is documentation, not
the coupling below.)

1. **The BSD reject scripts grep the rejection MESSAGE.**
   `.ci-scripts/openbsd-reject-unsupported-builds.sh` and
   `dragonfly-reject-unsupported-builds.sh` run a configure with an unsupported `use=` and
   `grep -q "not supported on <OS>"`. Three things silently make that grep miss — the reject
   then "passes" and an unsupported build is no longer rejected, with no other signal:
   - **Message wording and width.** The substring `not supported on <OS>` must survive
     verbatim AND fall within the first ~78 columns — CMake's `message()` reflows at ~78, so a
     reword that pushes the phrase past the wrap splits it across lines and `grep -q` fails.
   - **dtrace ordering.** For `dtrace`, the OS rejection must run BEFORE the
     `find_program(dtrace)` check, or on a BSD without the tool the message becomes "No dtrace
     compatible ..." and the grep fails.
   - **The rejected-option list.** Each script enumerates the options it expects rejected; add
     or drop a platform rejection in the module and the matching script's loop must change too.

2. **Include placement.** `include(cmake/PonyUses.cmake)` stays between `project()` and
   `find_package(LLVM REQUIRED)` in `CMakeLists.txt`. Below `project()` the OS checks read an
   unset `CMAKE_HOST_SYSTEM_NAME` and silently STOP rejecting; below `find_package` a bad
   `use=` fails with "LLVM not found" instead of the platform message the reject scripts grep
   for. (The reject scripts also configure into a throwaway build dir and delete it, so a
   base-compiler reject never pins the shared release build to the wrong compiler.)

3. **A new option needs two edits.** Add it to the module AND to a consumption block in
   `CMakeLists.txt` (or `src/libponyrt/CMakeLists.txt` for `PONY_USE_DTRACE`), or it validates
   but does nothing.

Run: the tier-3 BSD legs (`.github/workflows/ponyc-tier3.yml`: openbsd, dragonflybsd) exercise
the reject scripts; `ponyc-weekly-checks.yml` exercises the options on Linux/macOS.
