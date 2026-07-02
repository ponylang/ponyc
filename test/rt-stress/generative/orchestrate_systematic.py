#!/usr/bin/env python3
"""Systematic-testing stress orchestrator for the generative engine (main.pony).

Runs the engine under a `use=systematic_testing` ponyc: the runtime is serialized
to one thread at a time, and a fixed `--ponysystematictestingseed` replays one
scheduler interleaving, so any failure reproduces from a single number. This mode
explores interleavings deterministically; by construction it never runs two cores
at the same instant, so it cannot catch true-simultaneity bugs (atomic tearing,
memory ordering, real lock-free contention) -- that is orchestrate_normal.py's job.

From a master seed it:
  1. derives the program RNG seed (`--seed`) and the scheduler-interleaving seed
     (`--ponysystematictestingseed`, forced >= 1);
  2. draws a swarm configuration -- one of `mesh | cyclic | backpressure | iso` plus a
     random subset of the non-timing runtime knobs (including
     `--ponynoscale` and `--ponynoblock`, both drawn ~50% -- the oracle holds with
     the cycle detector on) and `--ponymaxthreads` across [1, the host's probed
     physical core count];
  3. compiles the engine once with the provided `--ponyc`;
  4. runs each seed TWICE under a watchdog and an address-space cap and requires
     the two runs' observable ORDER_SIG to match (the determinism oracle, always
     on -- a divergence is a race), plus exit code + timeout, writing a failure
     bundle. A seed replays from its number.

The mode-agnostic, cross-platform mechanism lives in stress_common.py; only the
systematic-specific config draw, the determinism double-run, and the guard live
here. resolve_config is pinned in orchestrate_systematic_test.py.

The ponyc passed in must be a debug + systematic build:

    make configure config=debug use=scheduler_scaling_pthreads,systematic_testing
    make build config=debug
    python3 test/rt-stress/generative/orchestrate_systematic.py \\
      --ponyc build/debug-scheduler_scaling_pthreads-systematic_testing/ponyc \\
      --use-flags scheduler_scaling_pthreads,systematic_testing \\
      --count 50 --out ~/tmp/rt-stress-out

`--version` does not echo the build's `use=` flags, so the bundle records
`--use-flags` separately.
"""
import argparse
import json
import os
import random
import shlex
import sys

import stress_common as common


def resolve_config(master_seed, max_threads):
    """Fully resolve a systematic run from a master seed and the host's physical
    core count. Every draw here -- the workload kind, its per-kind cargo, and
    `--ponynoblock` -- shifts the rng stream, so widening the draw beyond mesh
    REMAPPED every historical systematic seed: resolve_config(0, 8) is pinned in
    orchestrate_systematic_test.py and a seed from before that widening no longer
    reconstructs its old run.

    Draw order IS the seed-stability contract: the workload (draw_systematic_workload:
    kind + every kind's cargo, fixed consumption) -> `--ponynoblock` -> the four
    shared swarm knobs -> payload-mode -> ponymaxthreads last. `--ponynoblock` is a
    swarm knob here now (drawn ~50%), NOT forced: the determinism oracle holds with
    the cycle detector ON, because the detector's recipient-scheduling sends are
    sorted by a stable actor id so replay is layout-independent (verified across
    cyclic + iso; see .known-couplings/systematic-testing-send-ordering.md). Drawing
    it exercises both the detector-on path (cyclic collection, iso acquire) and the
    detector-off path -- the cycle-detector coverage the normal mode also gives, now
    under the reproducible oracle. `--ponynoscale` is also a swarm knob (both
    reproduce).
    """
    program_seed = common.derive_seed(master_seed, "program")
    systematic_seed = common.derive_seed(master_seed, "systematic")
    rng = random.Random(master_seed)

    workload = common.draw_systematic_workload(rng, program_seed)
    runtime = {"ponysystematictestingseed": systematic_seed}
    if rng.random() < 0.5:
        runtime["ponynoblock"] = True
    common.draw_swarm_knobs(rng, runtime)
    # payload-mode is drawn here (the seed-stability position), emitted only for the
    # kinds carrying a val payload -- iso draws-then-ignores it, like normal mode.
    mode = common.draw_payload_mode(rng)
    if workload["workload"] != "iso":
        workload["payload-mode"] = mode
    runtime["ponymaxthreads"] = common.draw_max_threads(rng, max_threads)

    return {"master_seed": master_seed, "workload": workload, "runtime": runtime}


def check_determinism(binary, config, version, use_flags, out_dir, timeout,
                      mem_limit_bytes):
    """Run one seed twice and compare the observable ORDER_SIG. Divergence at a
    fixed seed pair is a race. A non-pass run is a failure in its own right. On
    any determinism failure a `determinism-<seed>.json` bundle is written
    capturing both runs, so the divergence -- the headline oracle failure, which
    otherwise leaves no on-disk artifact since both runs exit 0 -- reproduces from
    the record, not just from stdout."""
    seed = config["master_seed"]
    first = common.execute(binary, config, version, use_flags, out_dir, timeout,
                           mem_limit_bytes)
    second = common.execute(binary, config, version, use_flags, out_dir, timeout,
                            mem_limit_bytes)
    sig_a = common.parse_result(first.stdout).get("order_sig")
    sig_b = common.parse_result(second.stdout).get("order_sig")
    runs_ok = (first.outcome == "pass") and (second.outcome == "pass")
    have_sigs = (sig_a is not None) and (sig_b is not None)

    if runs_ok and have_sigs and (sig_a == sig_b):
        common.info("[seed %d] determinism ok: ORDER_SIG=%s reproduced"
                    % (seed, sig_a))
        return True

    if not runs_ok:
        verdict = "a run did not pass (cannot compare ORDER_SIG)"
    elif not have_sigs:
        verdict = "a run exited 0 without an ORDER_SIG (cannot compare)"
    else:
        verdict = "DETERMINISM FAILURE: %s != %s (a race)" % (sig_a, sig_b)
    common.info("[seed %d] %s" % (seed, verdict))

    # Build the top-level bundle from the failing run when one failed (otherwise
    # the headline outcome would describe a passing run); the determinism block
    # below carries both runs regardless.
    failed = first if first.outcome != "pass" else second
    bundle = common.bundle_for(config, version, use_flags,
                               common.run_command(binary, config),
                               {"timeout_seconds": timeout,
                                "mem_limit_bytes": mem_limit_bytes}, failed)
    bundle["determinism"] = {"verdict": verdict, "sig_run1": sig_a,
                             "sig_run2": sig_b, "outcome_run1": first.outcome,
                             "outcome_run2": second.outcome}
    path = os.path.join(out_dir, "determinism-%d.json" % seed)
    with open(path, "w") as handle:
        json.dump(bundle, handle, indent=2, sort_keys=True)
    common.info("wrote determinism bundle: " + path)
    return False


def main(argv):
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ponyc", required=True,
                        help="path to a debug + systematic ponyc")
    parser.add_argument("--use-flags", required=True,
                        help="use= flags the ponyc was built with (provenance)")
    parser.add_argument("--out", help="output dir for the binary and bundles")
    parser.add_argument("--timeout", type=int,
                        default=common.DEFAULT_TIMEOUT_SECONDS,
                        help="per-run wall-clock watchdog, seconds (default %d)"
                        % common.DEFAULT_TIMEOUT_SECONDS)
    parser.add_argument("--mem-limit-mb", type=int,
                        default=common.DEFAULT_MEM_LIMIT_MB,
                        help="per-run address-space cap, MB (default %d); a cap "
                        "below the runtime's startup need reports as a FAIL"
                        % common.DEFAULT_MEM_LIMIT_MB)
    parser.add_argument("--start", type=int, default=1,
                        help="first master seed for --count (ignored otherwise)")
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--master-seed", type=int, help="run one master seed")
    source.add_argument("--count", type=int, help="run --start .. --start+N-1")
    source.add_argument("--seeds", help="comma-separated master seeds")
    source.add_argument("--replay", type=int,
                        help="re-run one master seed verbosely")
    args = parser.parse_args(argv)

    ponyc = os.path.abspath(args.ponyc)
    if not os.path.isfile(ponyc):
        common.die("ponyc not found at " + ponyc)
    if "systematic_testing" not in args.use_flags:
        common.die("--use-flags must include systematic_testing: the engine passes "
                   "--ponysystematictestingseed, which a non-systematic runtime "
                   "rejects (every seed would 'fail'). Use orchestrate_normal.py "
                   "for a normal runtime.")

    out_dir = os.path.abspath(args.out) if args.out else os.path.join(
        common.REPO_ROOT, "rt-stress-out")
    os.makedirs(out_dir, exist_ok=True)
    mem_limit_bytes = args.mem_limit_mb * 1024 * 1024

    version = common.ponyc_version(ponyc)
    common.info("ponyc: " + version.replace("\n", " | "))
    common.info("use-flags: " + args.use_flags)
    binary = common.compile_engine(ponyc, out_dir)
    max_threads = common.probe_max_threads(
        binary, ["--ponysystematictestingseed", "1"])
    common.info("physical cores (--ponymaxthreads ceiling): %d" % max_threads)

    seeds = common.resolve_seeds(args)
    if not seeds:
        common.die("no seeds to run -- --count must be >= 1 (a 0/negative --count "
                   "or empty --seeds runs nothing and would exit 0)")
    replay = args.replay is not None
    if replay:
        config = resolve_config(seeds[0], max_threads)
        common.info("replay config: " + json.dumps(config, sort_keys=True))
        common.info("replay cli: " + " ".join(
            shlex.quote(a) for a in common.run_command(binary, config)))

    failures = 0
    for seed in seeds:
        config = resolve_config(seed, max_threads)
        # Determinism is checked on every seed (always-on): each runs twice and
        # the observable ORDER_SIG must match. The second run is also a full
        # conservation/crash/liveness run, so it is not wasted.
        if not check_determinism(binary, config, version, args.use_flags,
                                 out_dir, args.timeout, mem_limit_bytes):
            failures += 1

    common.info("ran %d seed(s), %d failure(s)" % (len(seeds), failures))
    if failures != 0:
        sys.exit(1)


if __name__ == "__main__":
    main(sys.argv[1:])
