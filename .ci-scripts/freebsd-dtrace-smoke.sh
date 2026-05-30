#!/bin/sh
# use=dtrace smoke test for the FreeBSD tier-3 job. Runs inside
# the FreeBSD VM, invoked from .github/workflows/ponyc-tier3.yml. POSIX sh, not
# bash: FreeBSD's base system has no bash. Expects to run from the ponyc source
# root with doas configured (the tier-3 install step grants the build user
# passwordless doas so it can load the dtrace kernel module and run dtrace).
#
# Tier-3 otherwise never builds with dtrace, so FreeBSD's illumos dtrace path
# went uncovered. This builds a clean dtrace runtime and links+runs a program,
# which exercises both the runtime build (`dtrace -G`) and the
# `-ldtrace_probes` link path. Then, where the dtrace kernel device can be
# loaded, it asserts a `pony` provider probe actually fires.
set -eu

# Build the dtrace runtime from scratch. The earlier non-dtrace config=debug
# build left a build dir behind, and reconfiguring it in place re-runs the
# standalone-library rule over a read-only leftover (`libc++.a` is 0444, so the
# copied `libcpp.a` is too) which fails. `clean` is config-scoped and defaults
# to release, so pass config=debug to remove the debug build/output dirs; the
# prebuilt LLVM in build/libs is untouched.
gmake clean config=debug
gmake configure config=debug use=dtrace
gmake build config=debug

smoke=/tmp/dtrace-smoke
mkdir -p "$smoke"
cat > "$smoke/main.pony" <<'PONY'
actor Main
  new create(env: Env) =>
    var i: USize = 0
    while i < 2000 do
      Spinner
      i = i + 1
    end

actor Spinner
  be noop() => None
PONY

PONYPATH="$PWD/packages" ./build/debug-dtrace/ponyc -o "$smoke" "$smoke"
"$smoke/dtrace-smoke"

# If the VM can load the dtrace device, assert a pony provider probe actually
# fires. Tolerate a VM that can't load it (environment), but fail on a real
# regression where the probe is registered yet silent.
doas kldload dtraceall 2>/dev/null || true
if doas dtrace -l >/dev/null 2>&1; then
  out=$(doas dtrace -q -n 'pony*:::actor-alloc { @ = count(); } END { printa("ALLOC=%@u\n", @); }' -c "$smoke/dtrace-smoke" 2>&1)
  echo "$out"
  echo "$out" | grep -qE 'ALLOC=[1-9]' || { echo "FAIL: pony dtrace provider did not fire"; exit 1; }
  echo "pony dtrace provider fired"
else
  echo "dtrace device unavailable in CI VM; skipped probe-firing assertion"
fi
