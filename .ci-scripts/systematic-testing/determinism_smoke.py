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
  2. Compiles the test/rt-systematic fixtures with it. Each folds the message
     arrival order into an order-sensitive hash, ORDER_SIG, which is a pure
     function of the scheduler interleaving. order-signature exercises the base
     scheduler path; mute-order-signature additionally drives the actor
     muting/unmuting reschedule path (its foreign-send volume overloads actors);
     acquire-release-order-signature drives the ORCA reference-counting send path
     (it forwards fresh heap payloads across a spreading mesh, so actors release
     references owned by many distinct actors per GC sweep) and runs with the
     cycle detector disabled; cycle-collection-order-signature drives the cycle
     detector's own paths -- it forms and reclaims reference cycles with the
     detector left on and swept frequently (--ponycdinterval 10), exercising its
     probe, confirmation, deferred-detection, and collection sends -- see the
     per-fixture flags in FIXTURES below.
  3. Asserts the property, scoped to `--ponynoscale` (dynamic scheduler scaling
     off), at the runtime's default thread count (the host's physical cores),
     for each fixture:
       - reproducible: one seed, many runs, all the same ORDER_SIG;
       - still exploring: several seeds produce more than one distinct ORDER_SIG.

It asserts the *property*, never a specific ORDER_SIG value. The value is
platform-dependent (the scheduler draws from rand()/srand(), whose stream differs
by platform), so a hardcoded value would false-fail off this machine; the
property holds on any platform with more than one scheduler thread.

Scope and limits:
  - `--ponynoscale`: making dynamic scheduler scaling itself reproducible is
    follow-up work and is not checked here.
  - mute-order-signature only catches a muting-reschedule regression when actors
    actually overload, which needs load and thread count in balance. It is sized
    for the 2-4 physical cores a CI runner typically has; on a host with many
    more cores the per-thread load is too light to mute and that fixture degrades
    to a plain reproducibility check (still correct, just no longer guarding the
    muting path).
  - acquire-release-order-signature runs with the cycle detector disabled
    (--ponynoblock) to isolate the ORCA reference-counting (ACQUIRE/RELEASE) send
    ordering (#5568). Like the others, this check asserts reproducibility and
    exploration, not flakes-before-the-fix; that property was confirmed when the
    fixture was added (it diverged per run against the pre-#5568 runtime at every
    thread count tried, 2 through 16). Unlike mute-order-signature, it does not
    depend on load/core-count balance to flake, so it is not expected to degrade
    to a plain reproducibility check on many-core hosts -- but CI does not re-check
    that each run. The cycle detector's own send ordering is covered separately by
    cycle-collection-order-signature (below); acquire-release keeps the detector
    off so the two do not confound each other.
  - cycle-collection-order-signature runs with the cycle detector ENABLED and
    swept frequently (--ponycdinterval 10), and forms and reclaims reference
    cycles, so it drives all of the detector's pointer-ordered paths #5569 fixes:
    the blocked-actor probes (ACTORMSG_ISBLOCKED), per-cycle confirmations
    (ACTORMSG_CONF), the deferred-detection order, and collect()'s per-member
    release sends. What it OBSERVES is the probe / confirmation / deferred
    ordering; collect()'s cross-member order is exercised but not folded into
    ORDER_SIG (every cycle member references the same one Collector), so a
    collect-only ordering regression would not flake here -- see the fixture
    docstring. Like the others it asserts reproducibility and exploration, not
    flakes-before; that was confirmed by hand when the fixture was added (it
    diverged per run against the pre-#5569 runtime from two scheduler threads up
    -- most seeds at two threads, all tested seeds at four and at the host
    default; ASLR-off and detector-off both made it reproduce, isolating the
    cause to the detector). After the fix every seed reproduces, and the seeds
    still explore at the thread counts tried (two through the host default) --
    though, like mute-order-signature, that exploration is empirical: a core
    count where the fixed seeds collapse to one interleaving would surface as a
    false exploration failure, not a real regression. Reclamation runs during
    the program (also guarding the collection path against a crash regression),
    but is not sequenced before the final exit, so that crash guard is
    reliable-in-practice rather than guaranteed.
  - These fixtures do not cover every source of layout- or timing-dependence in
    the systematic scheduler. One cycle-detector path is layout-independent by
    construction but not exercised here: above CD_MAX_CHECK_BLOCKED live actors
    the systematic build disables check_blocked's rate limiter and probes all of
    d->views each sweep (so the resumption cursor never selects a layout-dependent
    subset), but no fixture drives that many actors. The detector's shutdown
    finalizer pass still runs in pointer order, but it runs at termination after
    all observable output and `_final` cannot send messages, so it cannot affect
    replay.

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

# (package, binary name, extra runtime flags) for each fixture. order-signature
# covers the base scheduler path; mute-order-signature additionally exercises the
# actor muting/unmuting reschedule path; acquire-release-order-signature exercises
# the ORCA reference-counting (ACQUIRE/RELEASE) send path and runs with
# --ponynoblock so the cycle detector's own sends cannot confound it;
# cycle-collection-order-signature forms and reclaims reference cycles with the
# detector on and runs with --ponycdinterval 10 so it sweeps often enough to
# exercise the detector's probe / confirmation / deferred / collection sends. All
# run at the runtime's default thread count (physical cores); see the module
# docstring for coverage ceilings.
FIXTURES = [
    ("test/rt-systematic/order-signature", "order-signature", []),
    ("test/rt-systematic/mute-order-signature", "mute-order-signature", []),
    ("test/rt-systematic/acquire-release-order-signature",
     "acquire-release-order-signature", ["--ponynoblock"]),
    ("test/rt-systematic/cycle-collection-order-signature",
     "cycle-collection-order-signature", ["--ponycdinterval", "10"]),
]
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


def compile_reproducer(root, ponyc, out_dir, package, name):
    env = dict(os.environ, PONYPATH=os.path.join(root, "packages"))
    result = run([ponyc, "-b", name, "--pic", "-o", out_dir,
                  os.path.join(root, package)],
                 cwd=root, env=env)
    if result.returncode != 0:
        fail("compiling %s with the systematic-testing ponyc failed" % name)
    binary = os.path.join(out_dir, name)
    if not os.access(binary, os.X_OK):
        fail("reproducer binary not produced at " + binary)
    return binary


def order_sig(binary, seed, extra_flags):
    """Run the reproducer once and return its ORDER_SIG, or fail loudly.

    extra_flags are the fixture's own runtime flags from FIXTURES (e.g.
    --ponynoblock for acquire-release-order-signature); they are appended after
    the shared --ponynoscale.

    stderr is captured (not discarded) so it can be surfaced on failure -- the
    systematic-testing banner goes to stderr, but so would any crash diagnostic.
    """
    cmd = ([binary, "--ponysystematictestingseed", str(seed), "--ponynoscale"]
           + extra_flags)
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


def check_fixture(binary, name, extra_flags):
    """Assert the reproducibility property for one compiled fixture."""
    # Reproducible: one seed, many runs, all identical.
    repro_sigs = [order_sig(binary, REPRODUCIBILITY_SEED, extra_flags)
                  for _ in range(REPRODUCIBILITY_RUNS)]
    if not is_reproducible(repro_sigs):
        fail("%s: seed %d gave %d distinct ORDER_SIG over %d runs (expected 1, "
             "so the interleaving is not reproducible): %s"
             % (name, REPRODUCIBILITY_SEED, len(set(repro_sigs)),
                REPRODUCIBILITY_RUNS, sorted(set(repro_sigs))))
    print("%s reproducible: seed %d gave one ORDER_SIG (%s) over %d runs"
          % (name, REPRODUCIBILITY_SEED, repro_sigs[0], REPRODUCIBILITY_RUNS))

    # Still exploring: several seeds must not collapse to one ordering.
    seed_sigs = [order_sig(binary, seed, extra_flags)
                 for seed in EXPLORATION_SEEDS]
    if not explores(seed_sigs):
        fail("%s: all %d seeds produced the same ORDER_SIG (%s); seed no longer "
             "drives the interleaving -- exploration has collapsed"
             % (name, len(EXPLORATION_SEEDS), seed_sigs[0]))
    print("%s still exploring: %d seeds produced %d distinct ORDER_SIG"
          % (name, len(EXPLORATION_SEEDS), len(set(seed_sigs))))


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
        for package, name, extra_flags in FIXTURES:
            binary = compile_reproducer(root, ponyc, out_dir, package, name)
            check_fixture(binary, name, extra_flags)

    print("systematic-testing reproducibility smoke test passed")


if __name__ == "__main__":
    main()
