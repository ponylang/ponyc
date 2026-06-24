#!/usr/bin/env python3
"""Weekly regression guard for systematic-testing reproducibility (#5560).

Systematic testing (`use=systematic_testing`) exists so a runtime interleaving
can be replayed from a seed. The reproducibility half of that only holds above
one scheduler thread once the scheduler's clock-gated decisions run off a
deterministic logical clock instead of the wall clock. Nothing in the normal CI
builds with `use=systematic_testing`, so a regression that reintroduced a
wall-clock read on the scheduler's control path -- or that broke the systematic
build outright -- would pass every PR and only surface when someone next tried to
replay a seed. This guards against that.

What it does, from the ponyc source root, in a container that has already built
build/libs:

  1. Builds a systematic-testing ponyc
     (`use=scheduler_scaling_pthreads,systematic_testing`). The build itself is
     the first assertion -- it is the only thing in CI that compiles that code
     path.
  2. Compiles test/rt-systematic/order-signature with it. That program folds the
     message arrival order into an order-sensitive hash, ORDER_SIG, which is a
     pure function of the scheduler interleaving.
  3. Asserts the property the fix guarantees, scoped to `--ponynoscale` (dynamic
     scheduler scaling off), with more than one scheduler thread:
       - reproducible: one seed, many runs, all the same ORDER_SIG;
       - still exploring: several seeds produce more than one distinct ORDER_SIG.

It asserts the *property*, never a specific ORDER_SIG value. The value is
platform-dependent (the scheduler draws from rand()/srand(), whose stream differs
by platform), so a hardcoded value would false-fail off this machine; the
property holds on any platform with more than one scheduler thread.

Scope is deliberately `--ponynoscale`. Making dynamic scheduler scaling itself
reproducible is follow-up work and is not checked here.

The pure pieces (parse_order_sig / is_reproducible / explores) are unit-tested in
determinism_smoke_test.py.
"""

import os
import re
import subprocess
import sys
import tempfile

# A fixed seed must replay the same interleaving across this many runs.
REPRODUCIBILITY_RUNS = 8
# Distinct seeds that must, between them, produce more than one interleaving
# (so we know pinning the seed didn't flatten the search to a single ordering).
EXPLORATION_SEEDS = [12345, 111, 222, 333, 444, 555, 98765, 24680]
REPRODUCIBILITY_SEED = 12345
# Each run is serialized to one thread at a time; a healthy run finishes well
# under this. A run that exceeds it is a hang, which is itself a failure.
RUN_TIMEOUT_SECONDS = 120

ORDER_SIG_RE = re.compile(rb"ORDER_SIG=(\d+)")
REPRO_PACKAGE = "test/rt-systematic/order-signature"
# The output suffix order (scheduler_scaling_pthreads then systematic_testing)
# comes from the block order of the PONY_USE_* `if()`s in the top-level
# CMakeLists.txt, NOT from the order passed to `use=`. If those blocks are
# reordered this path goes stale; the os.access check below then fails loudly.
PONYC_REL = "build/debug-scheduler_scaling_pthreads-systematic_testing/ponyc"


def parse_order_sig(output):
    """Return the ORDER_SIG value from a run's stdout, or None if absent."""
    match = ORDER_SIG_RE.search(output)
    return match.group(1).decode() if match is not None else None


def is_reproducible(sigs):
    """True if every run produced the same (non-empty) signature."""
    return len(sigs) > 0 and len(set(sigs)) == 1


def explores(sigs):
    """True if the signatures span more than one ordering (search not collapsed)."""
    return len(set(sigs)) >= 2


def run(cmd, **kwargs):
    print("+ " + " ".join(cmd), flush=True)
    return subprocess.run(cmd, **kwargs)


def fail(message):
    print("FAIL: " + message, file=sys.stderr, flush=True)
    sys.exit(1)


def build_systematic_ponyc(root):
    # The build is itself an assertion: this is the only place CI compiles the
    # systematic-testing code path. A failure here is a real regression.
    #
    # No arch= is passed, so the Makefile default (native) is used. The check
    # asserts a property and builds + runs on the same machine, so native is
    # self-consistent, and leaving arch out keeps the script usable as-is if the
    # job grows to other architectures.
    if run(["make", "configure", "config=debug",
            "use=scheduler_scaling_pthreads,systematic_testing"],
           cwd=root).returncode != 0:
        fail("`make configure` for the systematic-testing build failed")
    if run(["make", "build", "config=debug"], cwd=root).returncode != 0:
        fail("`make build` for the systematic-testing build failed")

    ponyc = os.path.join(root, PONYC_REL)
    if not os.access(ponyc, os.X_OK):
        fail("systematic-testing ponyc not found at " + PONYC_REL)
    return ponyc


def compile_reproducer(root, ponyc, out_dir):
    env = dict(os.environ, PONYPATH=os.path.join(root, "packages"))
    result = run([ponyc, "-b", "probe", "--pic", "-o", out_dir,
                  os.path.join(root, REPRO_PACKAGE)],
                 cwd=root, env=env)
    if result.returncode != 0:
        fail("compiling the reproducer with the systematic-testing ponyc failed")
    probe = os.path.join(out_dir, "probe")
    if not os.access(probe, os.X_OK):
        fail("reproducer binary not produced at " + probe)
    return probe


def order_sig(probe, seed):
    """Run the reproducer once and return its ORDER_SIG, or fail loudly.

    stderr is captured (not discarded) so it can be surfaced on failure -- the
    systematic-testing banner goes to stderr, but so would any crash diagnostic.
    """
    cmd = [probe, "--ponysystematictestingseed", str(seed), "--ponynoscale"]
    try:
        result = run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                     timeout=RUN_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        fail("run hung (>%ds) at seed %d -- a hang is a regression"
             % (RUN_TIMEOUT_SECONDS, seed))
    if result.returncode != 0:
        fail("reproducer exited %d at seed %d; stderr:\n%s"
             % (result.returncode, seed,
                result.stderr.decode(errors="replace")))
    sig = parse_order_sig(result.stdout)
    if sig is None:
        fail("no ORDER_SIG in output at seed %d; stdout: %r; stderr:\n%s"
             % (seed, result.stdout, result.stderr.decode(errors="replace")))
    return sig


def main():
    root = os.getcwd()

    # The whole point is reproducibility across more than one scheduler thread.
    # --ponynoscale keeps the started thread count (CPU count) but turns dynamic
    # scaling off; with one CPU there is only one thread and nothing to check, so
    # skip rather than emit a misleading pass or a false failure. os.cpu_count()
    # is a host-side proxy for the count the runtime detects; they agree on the
    # GitHub-hosted runners this job uses (no cgroup pinning to a single CPU).
    cpus = os.cpu_count() or 1
    if cpus < 2:
        print("SKIP: only %d CPU available; the multi-thread reproducibility "
              "property needs more than one scheduler thread." % cpus)
        return

    ponyc = build_systematic_ponyc(root)

    with tempfile.TemporaryDirectory(prefix="systematic-repro-") as out_dir:
        probe = compile_reproducer(root, ponyc, out_dir)

        # Reproducible: one seed, many runs, all identical.
        repro_sigs = [order_sig(probe, REPRODUCIBILITY_SEED)
                      for _ in range(REPRODUCIBILITY_RUNS)]
        if not is_reproducible(repro_sigs):
            fail("seed %d gave %d distinct ORDER_SIG over %d runs (expected 1, "
                 "so the interleaving is not reproducible): %s"
                 % (REPRODUCIBILITY_SEED, len(set(repro_sigs)),
                    REPRODUCIBILITY_RUNS, sorted(set(repro_sigs))))
        print("reproducible: seed %d gave one ORDER_SIG (%s) over %d runs"
              % (REPRODUCIBILITY_SEED, repro_sigs[0], REPRODUCIBILITY_RUNS))

        # Still exploring: several seeds must not collapse to one ordering.
        seed_sigs = [order_sig(probe, seed) for seed in EXPLORATION_SEEDS]
        if not explores(seed_sigs):
            fail("all %d seeds produced the same ORDER_SIG (%s); seed no longer "
                 "drives the interleaving -- exploration has collapsed"
                 % (len(EXPLORATION_SEEDS), seed_sigs[0]))
        print("still exploring: %d seeds produced %d distinct ORDER_SIG"
              % (len(EXPLORATION_SEEDS), len(set(seed_sigs))))

    print("systematic-testing reproducibility smoke test passed")


if __name__ == "__main__":
    main()
