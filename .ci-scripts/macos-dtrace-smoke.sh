#!/bin/bash
# use=dtrace smoke test for macOS. Invoked from
# .github/workflows/ponyc-weekly-checks.yml. Expects to run from the ponyc
# source root with LLVM already built in build/libs.
#
# macOS dtrace has no `-G` flag — the LLD MachO linker resolves
# __dtrace_probe$ symbols natively, so no probe object is needed. This builds
# a dtrace-enabled ponyc and links+runs a small program, exercising the
# `dtrace -h` header generation and the embedded ld64.lld link path.
#
# This does NOT test that probes actually fire at runtime. Observing probes
# with `dtrace` requires System Integrity Protection (SIP) to permit DTrace,
# and GitHub Actions macOS runners have SIP fully enabled. The FreeBSD tier-3
# smoke can test probe firing because it controls the VM; here we can only
# validate the build and link path.
set -eu

# Clean + reconfigure debug from scratch. If a prior non-dtrace config=debug
# build left a build dir behind, reconfiguring it in place re-runs the
# standalone-library rule over a read-only leftover (libc++.a is 0444, so the
# copied libcpp.a is too) which fails. clean is config-scoped and defaults to
# release, so pass config=debug to remove the debug build/output dirs; the
# prebuilt LLVM in build/libs is untouched.
make clean config=debug
make configure config=debug use=dtrace
make build config=debug

# Derive the output directory rather than hardcode the suffix.
out=$(find build -maxdepth 1 -type d -name 'debug-dtrace' | head -1)
if [ -z "$out" ] || [ ! -x "$out/ponyc" ]; then
  echo "FAIL: dtrace build output directory not found"
  exit 1
fi

smoke=/tmp/dtrace-smoke
rm -rf "$smoke"
mkdir -p "$smoke"
cat > "$smoke/main.pony" <<'PONY'
actor Main
  new create(env: Env) =>
    var i: USize = 0
    while i < 2000 do
      Spinner
      i = i + 1
    end
    env.out.print("dtrace smoke ok")

actor Spinner
  be noop() => None
PONY

# Link at -V 3 (VERBOSITY_TOOL_INFO) so the linker invocation is printed, and
# assert it went through embedded LLD. The grep target is `ld64.lld` (the
# MachO LLD argv[0], distinct from the ELF `ld.lld` the FreeBSD smoke checks).
PONYPATH="$PWD/packages" "$out/ponyc" -V 3 -o "$smoke" "$smoke" \
  2>"$smoke/link.log"
grep -q 'ld64\.lld' "$smoke/link.log" || {
  echo "FAIL: dtrace link did not route through embedded LLD (no ld64.lld)"; exit 1; }
echo "dtrace link routed through embedded LLD"
out_text=$("$smoke/dtrace-smoke")
echo "$out_text"
echo "$out_text" | grep -q "dtrace smoke ok"
