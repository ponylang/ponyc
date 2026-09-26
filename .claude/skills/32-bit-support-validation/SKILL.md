---
name: 32-bit-support-validation
description: Load when validating ponyc 32-bit support. Covers syncing the test machine, building LLVM and ponyc, and running the full test suite on a 32-bit ARM RPi4.
disable-model-invocation: false
---

# Validating 32-bit support

Run the test suite on a 32-bit ARM machine to verify nothing is broken.
The default test machine is `pony-rpi4-32` (user `pi`, checkout at
`/home/pi/code/ponylang/ponyc`). It has a persistent checkout and a built LLVM
that only needs rebuilding when the vendored LLVM changes.

This is slow — LLVM builds take many hours, ponyc builds take roughly an hour each,
and the test suites add more on top. Run long steps detached and poll their logs.

## Known ILP32 failures

The 32-bit address space (~3 GB user) constrains what can be compiled and
linked as a single binary. These are current failures, not accepted
limitations — they need to be fixed.

- **The monolithic stdlib test** (`ctest -R stdlib`) compiles every stdlib
  package into one binary. LLVM runs out of memory during code generation
  (the "Function prototypes" phase), regardless of LTO mode, jemalloc, or
  debug/release codegen. Stdlib tests are run per-package instead
  (step 4c / 5c) as a workaround.

- **pony-doc** links against the full stdlib plus the pony_compiler
  library, exceeding the ILP32 link-time memory limit. The linker OOMs
  during the build. The remaining binaries (ponyc, pony-compiler,
  pony-lint, pony-lsp, pony-dep) build normally.

- **pony-doc-tests** cannot run because the binary cannot be built. The
  other four tool test suites run normally.

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
- **OpenSSL version matters for per-package tests.** cmake auto-detects the SSL
  library for ctest, but per-package compilation (steps 4c/5c) needs the flag
  passed explicitly. The scripts detect it via `openssl version`.
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

Two runtime builds (debug and release), each running the tests twice: once with
`PONY_DEBUG=1` (debug codegen) and once without (release codegen). The env var
controls whether ponyc compiles Pony sources with `-d`.

Each runtime build runs these tests in both codegen modes:

**ci-core minus stdlib** (run via ctest, excludes the monolithic stdlib test):

- **check-version** — ponyc `--version` exits successfully
- **libponyc.tests** — compiler C/C++ unit tests (GTest)
- **libponyrt.tests** — runtime C/C++ unit tests (GTest)
- **full-programs** — compile-and-run integration tests
- **full-program-runner-rejects-broken-config** — verifies the test runner rejects bad config
- **validate-grammar** — `pony.g` validated against the compiler

**stdlib per-package** (run via ponyc directly, one package at a time):

Each stdlib package is compiled and tested individually to stay within the
ILP32 address space limit. The full list of testable packages is derived from
`packages/stdlib/_test.pony`.

**tools** (4 of 5 tests; pony-doc-tests cannot run until the ILP32
linker OOM is fixed):

- **pony-compiler-tests** — self-hosted compiler tool tests
- **pony-dep-tests** — dependency tool tests
- **pony-lint-tests** — linter tool tests
- **pony-lsp-tests** — language server tool tests

## Step 4 — build and test with debug runtime

### 4a. Configure

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

### 4b. Build ponyc (detached)

The build will fail with a nonzero exit code because pony-doc OOMs during
linking (see "Known ILP32 failures"). After the build finishes, verify that
ponyc, pony-compiler, pony-dep, pony-lint, and pony-lsp were built
successfully.

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

Wait for `DEBUG_BUILD_EXIT_CODE` to appear (it will be nonzero). Then verify:

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cd /home/pi/code/ponylang/ponyc/build/debug
for bin in ponyc pony-compiler pony-dep pony-lint pony-lsp; do
  test -x "$bin" && echo "$bin: OK" || echo "$bin: MISSING"
done
for bin in pony-doc; do
  test -x "$bin" && echo "$bin: OK" || echo "$bin: MISSING (known ILP32 failure)"
done
ENDSSH
```

ponyc, pony-compiler, pony-dep, pony-lint, and pony-lsp must all show OK.
pony-doc will show MISSING until the ILP32 linker OOM is fixed.

### 4c. Run stdlib tests per-package (debug runtime)

Detect the SSL flag, then compile and run each testable package individually
in both codegen modes. Run detached — this iterates over ~30 packages.

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-debug-stdlib-perpkg.sh << 'SCRIPT'
#!/bin/bash
set -u
cd /home/pi/code/ponylang/ponyc

PONYC=./build/debug/ponyc
OUTDIR=./build/debug
PKGDIR=./packages

# Detect OpenSSL version for the SSL flag
ssl_ver=$(openssl version 2>/dev/null | head -1)
case "$ssl_ver" in
  OpenSSL\ 3.*) SSL_FLAG=-Dopenssl_3.0.x ;;
  OpenSSL\ 1.1.*) SSL_FLAG=-Dopenssl_1.1.x ;;
  LibreSSL*) SSL_FLAG=-Dlibressl ;;
  *) echo "ERROR: cannot detect OpenSSL version: $ssl_ver" >&2; exit 1 ;;
esac
echo "Detected SSL: $ssl_ver -> $SSL_FLAG"

# Packages with tests, from packages/stdlib/_test.pony
PACKAGES=(
  actor_pinning
  encode/base64
  buffered
  builtin_test
  bureaucracy
  cli
  collections
  collections/persistent
  constrained_types
  crypto
  files
  format
  http_client
  ini
  iregex
  itertools
  json
  math
  net
  pony_check
  pony_test
  process
  promises
  random
  runtime_info
  signals
  strings
  term
  time
  uri
)

LOGFILE=/home/pi/debug-stdlib-perpkg.log
> "$LOGFILE"
overall_rc=0

for codegen in debug release; do
  echo "=== $codegen codegen ===" >> "$LOGFILE"
  for pkg in "${PACKAGES[@]}"; do
    name=$(echo "$pkg" | tr '/' '_')
    debug_flag=""
    if [ "$codegen" = "debug" ]; then
      debug_flag="-d"
    fi

    echo -n "  $pkg ($codegen): " >> "$LOGFILE"

    # Compile
    $PONYC $debug_flag -b stdlib_test --checktree $SSL_FLAG --pic \
      "$PKGDIR/$pkg" -o "$OUTDIR" >> "$LOGFILE" 2>&1
    if [ $? -ne 0 ]; then
      echo "COMPILE FAILED" >> "$LOGFILE"
      overall_rc=1
      continue
    fi

    # Run
    "$OUTDIR/stdlib_test" --sequential >> "$LOGFILE" 2>&1
    if [ $? -ne 0 ]; then
      echo "TEST FAILED" >> "$LOGFILE"
      overall_rc=1
      continue
    fi

    echo "PASS" >> "$LOGFILE"
    rm -f "$OUTDIR/stdlib_test"
  done
done

echo "DEBUG_STDLIB_PERPKG_EXIT_CODE=$overall_rc" >> "$LOGFILE"
SCRIPT
chmod +x /home/pi/run-debug-stdlib-perpkg.sh
nohup /home/pi/run-debug-stdlib-perpkg.sh > /dev/null 2>&1 &
echo "debug per-package stdlib tests started pid $!"
ENDSSH
```

Poll:

```bash
ssh pi@pony-rpi4-32 bash -c '"tail -5 /home/pi/debug-stdlib-perpkg.log"'
```

Wait for `DEBUG_STDLIB_PERPKG_EXIT_CODE=0`.

### 4d. Run ci-core tests minus stdlib (debug runtime)

Run the ci-core tests excluding the monolithic stdlib, in both codegen modes.

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-debug-ci-core.sh << 'SCRIPT'
#!/bin/bash
cd /home/pi/code/ponylang/ponyc
echo "=== debug codegen ===" > /home/pi/debug-ci-core.log
PONY_DEBUG=1 ctest --preset debug -L ci-core -E "^stdlib$" >> /home/pi/debug-ci-core.log 2>&1
rc1=$?
echo "=== release codegen ===" >> /home/pi/debug-ci-core.log
ctest --preset debug -R "^full-programs$" >> /home/pi/debug-ci-core.log 2>&1
rc2=$?

if [ $rc1 -eq 0 ] && [ $rc2 -eq 0 ]; then
  echo "DEBUG_CI_CORE_EXIT_CODE=0" >> /home/pi/debug-ci-core.log
else
  echo "DEBUG_CI_CORE_EXIT_CODE=1 (debug_codegen=$rc1 release_codegen=$rc2)" >> /home/pi/debug-ci-core.log
fi
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

Wait for `DEBUG_CI_CORE_EXIT_CODE=0`.

### 4e. Build tool test binaries (after ponyc build completes)

Build only the tool test binaries that can link on ILP32. The `tool-tests`
target tries all five and will fail on pony-doc-tests (linker OOM).
Build the four that work individually instead.

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-debug-tool-build.sh << 'SCRIPT'
#!/bin/bash
cd /home/pi/code/ponylang/ponyc
rc=0
for target in pony-compiler-tests pony-dep-tests pony-lint-tests pony-lsp-tests; do
  echo "=== building $target ===" >> /home/pi/debug-tool-build.log
  cmake --build --preset debug --target "$target" >> /home/pi/debug-tool-build.log 2>&1
  if [ $? -ne 0 ]; then
    echo "$target: BUILD FAILED" >> /home/pi/debug-tool-build.log
    rc=1
  fi
done
echo "TOOL_BUILD_EXIT_CODE=$rc" >> /home/pi/debug-tool-build.log
SCRIPT
chmod +x /home/pi/run-debug-tool-build.sh
nohup /home/pi/run-debug-tool-build.sh > /dev/null 2>&1 &
echo "tool build started pid $!"
ENDSSH
```

Poll and wait for `TOOL_BUILD_EXIT_CODE=0`.

### 4f. Run tool tests (debug runtime)

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-debug-tools.sh << 'SCRIPT'
#!/bin/bash
cd /home/pi/code/ponylang/ponyc
rc=0
for test in pony-compiler-tests pony-dep-tests pony-lint-tests pony-lsp-tests; do
  ctest --preset debug -R "^${test}$" >> /home/pi/debug-tools.log 2>&1
  if [ $? -ne 0 ]; then rc=1; fi
done
echo "DEBUG_TOOLS_EXIT_CODE=$rc" >> /home/pi/debug-tools.log
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

Wait for `DEBUG_TOOLS_EXIT_CODE=0`.

## Step 5 — build and test with release runtime

Repeat the full test matrix with a release-built runtime and compiler.

### 5a. Configure

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

### 5b. Build ponyc (detached)

Same as step 4b — pony-doc will OOM (known ILP32 failure); verify the
other five binaries built.

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

Poll and wait for `RELEASE_BUILD_EXIT_CODE` to appear. Verify binaries as in 4b
(using `build/release` instead of `build/debug`).

### 5c. Run stdlib tests per-package (release runtime)

Same as step 4c but using the release ponyc.

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-release-stdlib-perpkg.sh << 'SCRIPT'
#!/bin/bash
set -u
cd /home/pi/code/ponylang/ponyc

PONYC=./build/release/ponyc
OUTDIR=./build/release
PKGDIR=./packages

# Detect OpenSSL version for the SSL flag
ssl_ver=$(openssl version 2>/dev/null | head -1)
case "$ssl_ver" in
  OpenSSL\ 3.*) SSL_FLAG=-Dopenssl_3.0.x ;;
  OpenSSL\ 1.1.*) SSL_FLAG=-Dopenssl_1.1.x ;;
  LibreSSL*) SSL_FLAG=-Dlibressl ;;
  *) echo "ERROR: cannot detect OpenSSL version: $ssl_ver" >&2; exit 1 ;;
esac
echo "Detected SSL: $ssl_ver -> $SSL_FLAG"

PACKAGES=(
  actor_pinning
  encode/base64
  buffered
  builtin_test
  bureaucracy
  cli
  collections
  collections/persistent
  constrained_types
  crypto
  files
  format
  http_client
  ini
  iregex
  itertools
  json
  math
  net
  pony_check
  pony_test
  process
  promises
  random
  runtime_info
  signals
  strings
  term
  time
  uri
)

LOGFILE=/home/pi/release-stdlib-perpkg.log
> "$LOGFILE"
overall_rc=0

for codegen in debug release; do
  echo "=== $codegen codegen ===" >> "$LOGFILE"
  for pkg in "${PACKAGES[@]}"; do
    name=$(echo "$pkg" | tr '/' '_')
    debug_flag=""
    if [ "$codegen" = "debug" ]; then
      debug_flag="-d"
    fi

    echo -n "  $pkg ($codegen): " >> "$LOGFILE"

    $PONYC $debug_flag -b stdlib_test --checktree $SSL_FLAG --pic \
      "$PKGDIR/$pkg" -o "$OUTDIR" >> "$LOGFILE" 2>&1
    if [ $? -ne 0 ]; then
      echo "COMPILE FAILED" >> "$LOGFILE"
      overall_rc=1
      continue
    fi

    "$OUTDIR/stdlib_test" --sequential >> "$LOGFILE" 2>&1
    if [ $? -ne 0 ]; then
      echo "TEST FAILED" >> "$LOGFILE"
      overall_rc=1
      continue
    fi

    echo "PASS" >> "$LOGFILE"
    rm -f "$OUTDIR/stdlib_test"
  done
done

echo "RELEASE_STDLIB_PERPKG_EXIT_CODE=$overall_rc" >> "$LOGFILE"
SCRIPT
chmod +x /home/pi/run-release-stdlib-perpkg.sh
nohup /home/pi/run-release-stdlib-perpkg.sh > /dev/null 2>&1 &
echo "release per-package stdlib tests started pid $!"
ENDSSH
```

Poll and wait for `RELEASE_STDLIB_PERPKG_EXIT_CODE=0`.

### 5d. Run ci-core tests minus stdlib (release runtime)

Same as step 4d but using the release preset.

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-release-ci-core.sh << 'SCRIPT'
#!/bin/bash
cd /home/pi/code/ponylang/ponyc
echo "=== debug codegen ===" > /home/pi/release-ci-core.log
PONY_DEBUG=1 ctest --preset release -L ci-core -E "^stdlib$" >> /home/pi/release-ci-core.log 2>&1
rc1=$?
echo "=== release codegen ===" >> /home/pi/release-ci-core.log
ctest --preset release -R "^full-programs$" >> /home/pi/release-ci-core.log 2>&1
rc2=$?

if [ $rc1 -eq 0 ] && [ $rc2 -eq 0 ]; then
  echo "RELEASE_CI_CORE_EXIT_CODE=0" >> /home/pi/release-ci-core.log
else
  echo "RELEASE_CI_CORE_EXIT_CODE=1 (debug_codegen=$rc1 release_codegen=$rc2)" >> /home/pi/release-ci-core.log
fi
SCRIPT
chmod +x /home/pi/run-release-ci-core.sh
nohup /home/pi/run-release-ci-core.sh > /dev/null 2>&1 &
echo "ci-core tests started pid $!"
ENDSSH
```

Poll and wait for `RELEASE_CI_CORE_EXIT_CODE=0`.

### 5e. Build and run tool tests (release runtime)

Same as steps 4e/4f but using the release preset.

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-release-tool-build.sh << 'SCRIPT'
#!/bin/bash
cd /home/pi/code/ponylang/ponyc
rc=0
for target in pony-compiler-tests pony-dep-tests pony-lint-tests pony-lsp-tests; do
  echo "=== building $target ===" >> /home/pi/release-tool-build.log
  cmake --build --preset release --target "$target" >> /home/pi/release-tool-build.log 2>&1
  if [ $? -ne 0 ]; then
    echo "$target: BUILD FAILED" >> /home/pi/release-tool-build.log
    rc=1
  fi
done
echo "TOOL_BUILD_EXIT_CODE=$rc" >> /home/pi/release-tool-build.log
SCRIPT
chmod +x /home/pi/run-release-tool-build.sh
nohup /home/pi/run-release-tool-build.sh > /dev/null 2>&1 &
echo "tool build started pid $!"
ENDSSH
```

Poll and wait for `TOOL_BUILD_EXIT_CODE=0`.

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
cat > /home/pi/run-release-tools.sh << 'SCRIPT'
#!/bin/bash
cd /home/pi/code/ponylang/ponyc
rc=0
for test in pony-compiler-tests pony-dep-tests pony-lint-tests pony-lsp-tests; do
  ctest --preset release -R "^${test}$" >> /home/pi/release-tools.log 2>&1
  if [ $? -ne 0 ]; then rc=1; fi
done
echo "RELEASE_TOOLS_EXIT_CODE=$rc" >> /home/pi/release-tools.log
SCRIPT
chmod +x /home/pi/run-release-tools.sh
nohup /home/pi/run-release-tools.sh > /dev/null 2>&1 &
echo "tool tests started pid $!"
ENDSSH
```

Poll and wait for `RELEASE_TOOLS_EXIT_CODE=0`.

## Step 6 — report results

Summarize which steps passed and which failed. Report each result individually:

- LLVM version and whether it needed rebuilding
- Debug runtime build: pass/fail (ponyc, pony-compiler, pony-dep, pony-lint,
  pony-lsp built; pony-doc fails — known ILP32 linker OOM)
- Debug runtime — ci-core minus stdlib (debug codegen): pass/fail
- Debug runtime — ci-core minus stdlib (release codegen): pass/fail
- Debug runtime — stdlib per-package (debug codegen): pass/fail per package
- Debug runtime — stdlib per-package (release codegen): pass/fail per package
- Debug runtime — tool tests (pony-compiler, pony-dep, pony-lint, pony-lsp): pass/fail
- Release runtime build: pass/fail (pony-doc fails — known ILP32 linker OOM)
- Release runtime — ci-core minus stdlib (debug codegen): pass/fail
- Release runtime — ci-core minus stdlib (release codegen): pass/fail
- Release runtime — stdlib per-package (debug codegen): pass/fail per package
- Release runtime — stdlib per-package (release codegen): pass/fail per package
- Release runtime — tool tests: pass/fail

## Cleanup

Kill any orphaned build processes and remove log files when done:

```bash
ssh pi@pony-rpi4-32 bash << 'ENDSSH'
pkill -f "cmake --build" 2>/dev/null
pkill -f "ctest --preset" 2>/dev/null
pkill -f "run-.*\.sh" 2>/dev/null
rm -f /home/pi/run-*.sh /home/pi/*-build.log /home/pi/*-tool-build.log /home/pi/*-ci-core.log /home/pi/*-tools.log /home/pi/*-stdlib-perpkg.log
ENDSSH
```

Do not remove the `build/` directory — it holds the built LLVM and ponyc, which
persist for the next validation run.
