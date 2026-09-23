---
name: 32-bit-support-validation
description: Load when validating ponyc 32-bit support. Covers syncing the test machine, building LLVM and ponyc, and running the full test suite on a 32-bit ARM RPi4.
disable-model-invocation: false
---

# Validating 32-bit support

Run the full test suite on a 32-bit ARM machine to verify nothing is broken.
The default test machine is `pony-rpi4-32` (user `pi`, checkout at
`/home/pi/code/ponylang/ponyc`). It has a persistent checkout and a built LLVM
that only needs rebuilding when the vendored LLVM changes.

This is slow — LLVM builds take many hours, ponyc builds take roughly an hour each,
and the test suites add more on top. Run long steps detached and poll their logs.

## Gotchas (read first)

- **Check the login shell.** If the machine's login shell is not bash (e.g. fish),
  every SSH command must go through bash explicitly — either
  `ssh pi@pony-rpi4-32 bash << 'ENDSSH'` for heredocs or
  `ssh pi@pony-rpi4-32 bash -c '...'` for one-liners.
- **Check the compiler.** The cmake presets default to `clang`/`clang++`. If the
  machine only has GCC, override on every cmake configure:
  `-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++`.
- **`-mtune=generic` is invalid on ARM GCC.** The `native` preset sets
  `-march=native -mtune=generic`, but GCC on ARM rejects `generic`. Override:
  `"-DCMAKE_C_FLAGS=-march=native" "-DCMAKE_CXX_FLAGS=-march=native"`.
  The LLVM build (`lib/build-libs.cmake`) does not set these flags, so it is not
  affected.
- **Don't run concurrent builds to the same build directory.** If an SSH session
  times out mid-build, kill the orphaned cmake process before starting another —
  two builds writing to the same directory corrupt it.
- **Detach long builds.** Write a bash script to the machine, run it with `nohup`,
  and poll the log file. Don't use `tee` over SSH — it keeps the connection alive
  and the SSH session will time out. Redirect to a file instead.
- **OpenSSL version is auto-detected.** cmake detects the installed OpenSSL/LibreSSL
  version; no manual flag is needed.
- **LLVM build parallelism.** LLVM is memory-hungry. On machines with limited RAM,
  use `-DJOBS=2` (or lower) to avoid OOM during the LLVM build.


## Step 1 — verify the machine is reachable

```bash
ssh -o ConnectTimeout=5 pi@pony-rpi4-32 bash -c '"uname -m && cat /etc/os-release | head -2"'
```

If the machine is not reachable, stop and ask where to run the 32-bit validation.
You need: an SSH-accessible 32-bit ARM Linux machine with a ponyc checkout, GCC,
cmake ≥ 3.25, and Python 3.

## Step 2 — update the repo

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cd /home/pi/code/ponylang/ponyc
git fetch origin
git checkout main
git pull origin main
git submodule update --init lib/llvm/src
git log --oneline -3
git -C lib/llvm/src rev-parse HEAD
ENDSSH
```

Confirm the log shows the expected HEAD of main, and the submodule hash matches
`LLVM_DESIRED_HASH` in `lib/CMakeLists.txt`.

## Step 3 — ensure LLVM is built and current

LLVM is "current" when all three of these are true:

1. The submodule at `lib/llvm/src` is at the commit specified by `LLVM_DESIRED_HASH`
   in `lib/CMakeLists.txt`.
2. The patches in `lib/llvm/patches/` have the hash specified by
   `PATCHES_DESIRED_HASH` in `lib/CMakeLists.txt`.
3. `build/libs/` exists and contains the LLVM built from that source with those
   patches applied (subdirectories `lib/`, `bin/`, `include/`).

### Check the submodule hash

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cd /home/pi/code/ponylang/ponyc
echo "desired: $(grep 'set(LLVM_DESIRED_HASH' lib/CMakeLists.txt | grep -oP '(?<=")[^"]+(?=")')"
echo "actual:  $(git -C lib/llvm/src rev-parse HEAD)"
ENDSSH
```

These must match. If they don't, `git submodule update --init` in step 2 didn't work
— re-run it, or investigate why the submodule is at the wrong commit.

### Check whether build artifacts exist

```bash
ssh pi@pony-rpi4-32 bash -c '"ls /home/pi/code/ponylang/ponyc/build/libs/lib/ 2>/dev/null | head -3 || echo NO_LIBS"'
```

If `NO_LIBS`, a full rebuild is needed (8+ hours on the RPi — see "Full rebuild"
below).

### Validate the built LLVM against current source and patches

Even when `build/libs/` exists and the submodule hash matches, the built LLVM might
be stale — the patches could have changed, or the build could be left over from a
different branch. Run `lib/build-libs.cmake` to validate everything. It checks both
`LLVM_DESIRED_HASH` and `PATCHES_DESIRED_HASH` internally, resets the submodule
source tree, applies the current patches, and does an incremental build. If the source
and patches haven't changed, the build step is a no-op (minutes, not hours).

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cd /home/pi/code/ponylang/ponyc
cmake -DJOBS=2 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -P lib/build-libs.cmake 2>&1
ENDSSH
```

Three outcomes:

- **Completes with no errors** → LLVM is current, proceed to step 4.
- **Fails with a hash mismatch** (`Patch hash actual '...' does not match
  desired '...'`) → the patches changed since the last build. Do a full rebuild
  (see below).
- **Fails with a submodule hash error** → the submodule is wrong. Re-run
  `git submodule update --init lib/llvm/src` in step 2 and try again.

If this step takes more than a few minutes, it is doing a real rebuild — detach it
(see "Full rebuild" below) and poll.

### Full rebuild

When a clean LLVM build is needed (no `build/libs/`, or a hash mismatch), remove
the old build artifacts and rebuild from scratch. This takes 8+ hours on the RPi.

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-libs-build.sh << 'SCRIPT'
#!/bin/bash
cd /home/pi/code/ponylang/ponyc
rm -rf build/libs build/build_libs
cmake -DJOBS=2 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -P lib/build-libs.cmake > /home/pi/libs-build.log 2>&1
echo "LIBS_BUILD_EXIT_CODE=$?" >> /home/pi/libs-build.log
SCRIPT
chmod +x /home/pi/run-libs-build.sh
nohup /home/pi/run-libs-build.sh > /dev/null 2>&1 &
echo "libs build started pid $!"
ENDSSH
```

Poll until done:

```bash
ssh pi@pony-rpi4-32 bash -c '"tail -3 /home/pi/libs-build.log"'
```

Wait for `LIBS_BUILD_EXIT_CODE=0`. A nonzero exit code means the build failed —
read the log above the exit code line for the error.

## Test matrix

Two runtime builds (debug and release), each running the same tests. The tests
include both debug-mode and release-mode variants — "debug" and "release" here
refer to how the test programs themselves are compiled by ponyc, not the runtime
build type.

Each runtime build runs these tests:

**ci-core** (10 tests, built by the normal `cmake --build`):

- **check-version** — ponyc `--version` exits successfully
- **output-layout** — build output directory has the expected structure
- **libponyc.tests** — compiler C/C++ unit tests (GTest)
- **libponyrt.tests** — runtime C/C++ unit tests (GTest)
- **stdlib-debug** — stdlib test suite, compiled with `-d` (debug mode)
- **stdlib-release** — stdlib test suite, compiled without `-d` (release mode)
- **full-programs-debug** — compile-and-run integration tests, debug mode
- **full-programs-release** — compile-and-run integration tests, release mode
- **full-program-runner-rejects-broken-config** — verifies the test runner rejects bad config
- **validate-grammar** — `pony.g` validated against the compiler

**tools** (5 tests, binaries must be built explicitly with `--target tool-tests`):

- **pony-compiler-tests** — self-hosted compiler tool tests
- **pony-doc-tests** — documentation tool tests
- **pony-lint-tests** — linter tool tests
- **pony-lsp-tests** — language server tool tests
- **pony-dep-tests** — dependency tool tests

## Step 4 — build and test with debug runtime

### Configure

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cd /home/pi/code/ponylang/ponyc
rm -rf build/build_debug
cmake --preset debug \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ \
  "-DCMAKE_C_FLAGS=-march=native" \
  "-DCMAKE_CXX_FLAGS=-march=native"
ENDSSH
```

Verify "Configuring done" and "Build files have been written" in the output.

### Build ponyc (detached)

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-debug-build.sh << 'SCRIPT'
#!/bin/bash
cd /home/pi/code/ponylang/ponyc
cmake --build --preset debug > /home/pi/debug-build.log 2>&1
echo "DEBUG_BUILD_EXIT_CODE=$?" >> /home/pi/debug-build.log
SCRIPT
chmod +x /home/pi/run-debug-build.sh
nohup /home/pi/run-debug-build.sh > /dev/null 2>&1 &
echo "debug build started pid $!"
ENDSSH
```

Poll:

```bash
ssh pi@pony-rpi4-32 bash -c '"tail -3 /home/pi/debug-build.log"'
```

Wait for `DEBUG_BUILD_EXIT_CODE=0`.

### Build tool test binaries (after ponyc build completes)

The tool test binaries are Pony programs compiled by ponyc and must be built
separately. Wait for the ponyc build to complete before starting this.

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-debug-tool-build.sh << 'SCRIPT'
#!/bin/bash
cd /home/pi/code/ponylang/ponyc
cmake --build --preset debug --target tool-tests > /home/pi/debug-tool-build.log 2>&1
echo "TOOL_BUILD_EXIT_CODE=$?" >> /home/pi/debug-tool-build.log
SCRIPT
chmod +x /home/pi/run-debug-tool-build.sh
nohup /home/pi/run-debug-tool-build.sh > /dev/null 2>&1 &
echo "tool build started pid $!"
ENDSSH
```

Poll and wait for `TOOL_BUILD_EXIT_CODE=0`.

### Run ci-core tests (debug runtime)

The stdlib and full-program tests take a long time on the RPi — run detached.

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-debug-ci-core.sh << 'SCRIPT'
#!/bin/bash
cd /home/pi/code/ponylang/ponyc
ctest --preset debug -L ci-core > /home/pi/debug-ci-core.log 2>&1
echo "DEBUG_CI_CORE_EXIT_CODE=$?" >> /home/pi/debug-ci-core.log
SCRIPT
chmod +x /home/pi/run-debug-ci-core.sh
nohup /home/pi/run-debug-ci-core.sh > /dev/null 2>&1 &
echo "ci-core tests started pid $!"
ENDSSH
```

Poll:

```bash
ssh pi@pony-rpi4-32 bash -c '"tail -5 /home/pi/debug-ci-core.log"'
```

Wait for `DEBUG_CI_CORE_EXIT_CODE=0`. This runs all ten ci-core tests:
`check-version`, `output-layout`, `libponyc.tests`, `libponyrt.tests`,
`stdlib-debug`, `stdlib-release`, `full-programs-debug`, `full-programs-release`,
`full-program-runner-rejects-broken-config`, and `validate-grammar`.

### Run tool tests (debug runtime)

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-debug-tools.sh << 'SCRIPT'
#!/bin/bash
cd /home/pi/code/ponylang/ponyc
ctest --preset debug -L tools > /home/pi/debug-tools.log 2>&1
echo "DEBUG_TOOLS_EXIT_CODE=$?" >> /home/pi/debug-tools.log
SCRIPT
chmod +x /home/pi/run-debug-tools.sh
nohup /home/pi/run-debug-tools.sh > /dev/null 2>&1 &
echo "tool tests started pid $!"
ENDSSH
```

Poll:

```bash
ssh pi@pony-rpi4-32 bash -c '"tail -5 /home/pi/debug-tools.log"'
```

Wait for `DEBUG_TOOLS_EXIT_CODE=0`. This runs: `pony-compiler-tests`,
`pony-doc-tests`, `pony-lint-tests`, `pony-lsp-tests`, and `pony-dep-tests`.

## Step 5 — build and test with release runtime

Repeat the full test matrix with a release-built runtime and compiler.

### Configure

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cd /home/pi/code/ponylang/ponyc
rm -rf build/build_release
cmake --preset release \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ \
  "-DCMAKE_C_FLAGS=-march=native" \
  "-DCMAKE_CXX_FLAGS=-march=native"
ENDSSH
```

### Build ponyc (detached)

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-release-build.sh << 'SCRIPT'
#!/bin/bash
cd /home/pi/code/ponylang/ponyc
cmake --build --preset release > /home/pi/release-build.log 2>&1
echo "RELEASE_BUILD_EXIT_CODE=$?" >> /home/pi/release-build.log
SCRIPT
chmod +x /home/pi/run-release-build.sh
nohup /home/pi/run-release-build.sh > /dev/null 2>&1 &
echo "release build started pid $!"
ENDSSH
```

Poll and wait for `RELEASE_BUILD_EXIT_CODE=0`.

### Build tool test binaries (after ponyc build completes)

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-release-tool-build.sh << 'SCRIPT'
#!/bin/bash
cd /home/pi/code/ponylang/ponyc
cmake --build --preset release --target tool-tests > /home/pi/release-tool-build.log 2>&1
echo "TOOL_BUILD_EXIT_CODE=$?" >> /home/pi/release-tool-build.log
SCRIPT
chmod +x /home/pi/run-release-tool-build.sh
nohup /home/pi/run-release-tool-build.sh > /dev/null 2>&1 &
echo "tool build started pid $!"
ENDSSH
```

Poll and wait for `TOOL_BUILD_EXIT_CODE=0`.

### Run ci-core tests (release runtime)

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-release-ci-core.sh << 'SCRIPT'
#!/bin/bash
cd /home/pi/code/ponylang/ponyc
ctest --preset release -L ci-core > /home/pi/release-ci-core.log 2>&1
echo "RELEASE_CI_CORE_EXIT_CODE=$?" >> /home/pi/release-ci-core.log
SCRIPT
chmod +x /home/pi/run-release-ci-core.sh
nohup /home/pi/run-release-ci-core.sh > /dev/null 2>&1 &
echo "ci-core tests started pid $!"
ENDSSH
```

Poll:

```bash
ssh pi@pony-rpi4-32 bash -c '"tail -5 /home/pi/release-ci-core.log"'
```

Wait for `RELEASE_CI_CORE_EXIT_CODE=0`. This runs the same ten ci-core tests as
the debug runtime — now using the release-built ponyc.

### Run tool tests (release runtime)

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-release-tools.sh << 'SCRIPT'
#!/bin/bash
cd /home/pi/code/ponylang/ponyc
ctest --preset release -L tools > /home/pi/release-tools.log 2>&1
echo "RELEASE_TOOLS_EXIT_CODE=$?" >> /home/pi/release-tools.log
SCRIPT
chmod +x /home/pi/run-release-tools.sh
nohup /home/pi/run-release-tools.sh > /dev/null 2>&1 &
echo "tool tests started pid $!"
ENDSSH
```

Poll:

```bash
ssh pi@pony-rpi4-32 bash -c '"tail -5 /home/pi/release-tools.log"'
```

Wait for `RELEASE_TOOLS_EXIT_CODE=0`. This runs: `pony-compiler-tests`,
`pony-doc-tests`, `pony-lint-tests`, `pony-lsp-tests`, and `pony-dep-tests`.

## Step 6 — report results

Summarize which steps passed and which failed. A build or test failure means the
32-bit support has a problem that needs fixing. Report each result individually:

- LLVM version and whether it needed rebuilding
- Debug runtime build: pass/fail
- Debug runtime — check-version: pass/fail
- Debug runtime — output-layout: pass/fail
- Debug runtime — libponyc.tests: pass/fail
- Debug runtime — libponyrt.tests: pass/fail
- Debug runtime — stdlib-debug: pass/fail
- Debug runtime — stdlib-release: pass/fail
- Debug runtime — full-programs-debug: pass/fail
- Debug runtime — full-programs-release: pass/fail
- Debug runtime — full-program-runner-rejects-broken-config: pass/fail
- Debug runtime — validate-grammar: pass/fail
- Debug runtime — pony-compiler-tests: pass/fail
- Debug runtime — pony-doc-tests: pass/fail
- Debug runtime — pony-lint-tests: pass/fail
- Debug runtime — pony-lsp-tests: pass/fail
- Debug runtime — pony-dep-tests: pass/fail
- Release runtime build: pass/fail
- Release runtime — check-version: pass/fail
- Release runtime — output-layout: pass/fail
- Release runtime — libponyc.tests: pass/fail
- Release runtime — libponyrt.tests: pass/fail
- Release runtime — stdlib-debug: pass/fail
- Release runtime — stdlib-release: pass/fail
- Release runtime — full-programs-debug: pass/fail
- Release runtime — full-programs-release: pass/fail
- Release runtime — full-program-runner-rejects-broken-config: pass/fail
- Release runtime — validate-grammar: pass/fail
- Release runtime — pony-compiler-tests: pass/fail
- Release runtime — pony-doc-tests: pass/fail
- Release runtime — pony-lint-tests: pass/fail
- Release runtime — pony-lsp-tests: pass/fail
- Release runtime — pony-dep-tests: pass/fail

## Cleanup

Kill any orphaned build processes and remove log files when done:

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
pkill -f "cmake --build" 2>/dev/null
pkill -f "ctest --preset" 2>/dev/null
rm -f /home/pi/run-*.sh /home/pi/*-build.log /home/pi/*-tool-build.log /home/pi/*-ci-core.log /home/pi/*-tools.log
ENDSSH
```

Do not remove the `build/` directory — it holds the built LLVM and ponyc, which
persist for the next validation run.
