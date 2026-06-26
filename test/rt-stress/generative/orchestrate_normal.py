#!/usr/bin/env python3
"""Normal (real-parallel) stress orchestrator for the generative engine (main.pony).

Runs the engine under a normal, production-default ponyc -- the real
multi-threaded runtime, NOT `use=systematic_testing`. Many cores run at the same
instant, so this catches true-simultaneity bugs (atomic tearing, memory ordering,
real lock-free contention) the serialized systematic mode cannot. The cost: runs
are NOT reproducible -- two runs of the same seed differ -- so there is no
determinism oracle here (that is orchestrate_systematic.py's job). What survives:
the conservation invariant (the engine itself exits non-zero on
sent != received != expected), crash detection, and the wall-clock liveness
watchdog. Because a normal-mode crash does NOT reproduce, the engine runs under
lldb so the crash backtrace is captured in the moment -- it is the only artifact
you will get for a raced memory-corruption crash.

From a master seed it:
  1. derives the program RNG seed (`--seed`); NO systematic seed is derived or
     passed (a normal runtime rejects `--ponysystematictestingseed`);
  2. draws a swarm configuration -- workload shape plus a random subset of the
     runtime knobs, with `--ponynoblock` ALSO a swarm knob (so the cycle detector
     is stressed when it is absent -- the cycle-detector coverage the retired
     string-message ubench gave; note ubench's sustained single-process load is
     NOT reproduced here, that awaits the open/continuous mode) and
     `--ponymaxthreads` across [1, physical cores];
  3. compiles the engine once with the provided `--ponyc`;
  4. runs each seed ONCE under lldb and a watchdog (and, on POSIX, an
     address-space cap), so a crash leaves a backtrace in the captured output;
     decides pass/fail from the engine's exit code (conservation) + crash +
     timeout (liveness), writing a failure bundle that carries the backtrace.

CI uses --budget-seconds for a fixed wall-clock soak (seeds from --start
incrementing); --count / --seeds / --master-seed / --replay run a fixed set
locally. A failing seed replays individually with --replay, though the run is
non-deterministic. The mode-agnostic, cross-platform mechanism lives in
stress_common.py. resolve_config is pinned in orchestrate_normal_test.py.

The ponyc passed in is a normal debug build (no special use flags):

    make configure config=debug
    make build config=debug
    python3 test/rt-stress/generative/orchestrate_normal.py \\
      --ponyc build/debug/ponyc --budget-seconds 1800 --out ~/tmp/rt-stress-out
"""
import argparse
import json
import os
import random
import shlex
import shutil
import sys
import time

import stress_common as common


def resolve_config(master_seed, max_threads):
    """Fully resolve a normal-mode run from a master seed and the host's physical
    core count. NO systematic seed. `--ponynoblock` is a swarm knob here (drawn,
    not forced), so both the cycle-detector-on and -off paths are exercised --
    this is where the cycle detector gets its stress coverage now that
    string-message-ubench is retired.

    This mode is non-reproducible (real parallelism), so a seed maps to a
    *configuration* deterministically but not to an *execution*. The draw order is
    still pinned (orchestrate_normal_test.py) so a given seed yields a stable
    config: workload (five fields) -> ponynoblock -> the four shared swarm knobs
    -> payload-mode -> ponymaxthreads last. This contract is independent of the
    systematic driver's (a seed need not mean the same config across modes).
    """
    program_seed = common.derive_seed(master_seed, "program")
    rng = random.Random(master_seed)

    workload = common.resolve_workload(rng, program_seed)
    runtime = {}
    if rng.random() < 0.5:
        runtime["ponynoblock"] = True
    common.draw_swarm_knobs(rng, runtime)
    workload["payload-mode"] = common.draw_payload_mode(rng)
    runtime["ponymaxthreads"] = common.draw_max_threads(rng, max_threads)

    return {"master_seed": master_seed, "workload": workload, "runtime": runtime}


def main(argv):
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ponyc", required=True,
                        help="path to a normal (non-systematic) debug ponyc")
    parser.add_argument("--use-flags", default="",
                        help="use= flags the ponyc was built with (provenance; "
                        "normally empty for a normal build)")
    parser.add_argument("--lldb", default="lldb",
                        help="path to lldb (default: on PATH); the engine runs "
                        "under it so crash backtraces are captured, since a "
                        "normal-mode crash does not reproduce")
    parser.add_argument("--out", help="output dir for the binary and bundles")
    parser.add_argument("--timeout", type=int,
                        default=common.DEFAULT_TIMEOUT_SECONDS,
                        help="per-run wall-clock watchdog, seconds (default %d); "
                        "set to ~2x the slowest legitimate run"
                        % common.DEFAULT_TIMEOUT_SECONDS)
    parser.add_argument("--mem-limit-mb", type=int,
                        default=common.DEFAULT_MEM_LIMIT_MB,
                        help="per-run address-space cap, MB (POSIX only; on "
                        "Windows the CI host's OOM handling applies; default %d)"
                        % common.DEFAULT_MEM_LIMIT_MB)
    parser.add_argument("--start", type=int, default=1,
                        help="first master seed for --budget-seconds and --count")
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--budget-seconds", type=int,
                        help="run seeds from --start incrementing until this many "
                        "wall-clock seconds elapse (the CI soak mode)")
    source.add_argument("--master-seed", type=int, help="run one master seed")
    source.add_argument("--count", type=int, help="run --start .. --start+N-1")
    source.add_argument("--seeds", help="comma-separated master seeds")
    source.add_argument("--replay", type=int,
                        help="re-run one master seed (non-deterministic)")
    args = parser.parse_args(argv)

    ponyc = os.path.abspath(args.ponyc)
    if not os.path.isfile(ponyc):
        common.die("ponyc not found at " + ponyc)
    if "systematic_testing" in args.use_flags:
        common.die("--use-flags must NOT include systematic_testing: this is the "
                   "normal (real-parallel) runtime, which rejects "
                   "--ponysystematictestingseed. Use orchestrate_systematic.py for "
                   "a systematic build.")
    if shutil.which(args.lldb) is None:
        common.die("lldb not found ('%s'). The normal mode runs the engine under "
                   "lldb to capture crash backtraces (a normal-mode crash does not "
                   "reproduce). Install lldb or pass --lldb PATH." % args.lldb)

    out_dir = os.path.abspath(args.out) if args.out else os.path.join(
        common.REPO_ROOT, "rt-stress-out")
    os.makedirs(out_dir, exist_ok=True)
    mem_limit_bytes = args.mem_limit_mb * 1024 * 1024

    version = common.ponyc_version(ponyc)
    common.info("ponyc: " + version.replace("\n", " | "))
    common.info("use-flags: " + (args.use_flags or "(none)"))
    binary = common.compile_engine(ponyc, out_dir)
    # Refuse a systematic build: with no --ponysystematictestingseed it would run
    # SERIALIZED off a wall-clock seed, silently defeating real parallelism while
    # conservation still passes. The --use-flags guard above only catches it when
    # the operator passes the flag; this catches it from the binary itself.
    if common.is_systematic_build(binary):
        common.die("--ponyc is a systematic_testing build, but this is the normal "
                   "(real-parallel) mode -- it would run SERIALIZED and silently "
                   "defeat real parallelism. Build a normal `config=debug` ponyc "
                   "(no use= flags), or use orchestrate_systematic.py.")
    # No systematic seed in the probe: a normal runtime rejects that flag.
    max_threads = common.probe_max_threads(binary, [])
    common.info("physical cores (--ponymaxthreads ceiling): %d" % max_threads)

    # Run every seed under lldb so a crash is captured with a backtrace. This is
    # deliberate even though lldb startup dominates per-seed time (so the soak
    # covers fewer seeds): a normal-mode crash does NOT reproduce, so re-running a
    # crashed seed under lldb later would not recapture it -- it is lldb-now or
    # never. Don't "optimize" this to a direct run without solving crash capture
    # another way (e.g. core dump + post-mortem lldb).
    def runner(b, c, t, m):
        return common.run_under_lldb(b, c, args.lldb, t, m)

    failures = 0
    if args.budget_seconds is not None:
        if args.budget_seconds <= 0:
            common.die("--budget-seconds must be >= 1")
        deadline = time.monotonic() + args.budget_seconds
        common.info("wall-clock budget: %ds, seeds from %d"
                    % (args.budget_seconds, args.start))
        seed = args.start
        ran = 0
        # Each seed runs once; a failure is recorded and we keep sweeping until the
        # budget elapses (more diverse configs = more chances to surface a race),
        # so the run can collect several failure bundles in one soak.
        while time.monotonic() < deadline:
            config = resolve_config(seed, max_threads)
            result = common.execute(binary, config, version, args.use_flags,
                                    out_dir, args.timeout, mem_limit_bytes,
                                    runner=runner)
            ran += 1
            if result.outcome != "pass":
                failures += 1
            seed += 1
        common.info("budget elapsed: ran %d seed(s) from %d, %d failure(s)"
                    % (ran, args.start, failures))
    else:
        seeds = common.resolve_seeds(args)
        if not seeds:
            common.die("no seeds to run -- --count must be >= 1 (a 0/negative "
                       "--count or empty --seeds runs nothing and would exit 0)")
        if args.replay is not None:
            config = resolve_config(seeds[0], max_threads)
            common.info("replay config: " + json.dumps(config, sort_keys=True))
            common.info("replay cli: " + " ".join(
                shlex.quote(a) for a in common.run_command(binary, config)))
        for seed in seeds:
            config = resolve_config(seed, max_threads)
            result = common.execute(binary, config, version, args.use_flags,
                                    out_dir, args.timeout, mem_limit_bytes,
                                    runner=runner)
            if result.outcome != "pass":
                failures += 1
        common.info("ran %d seed(s), %d failure(s)" % (len(seeds), failures))

    if failures != 0:
        sys.exit(1)


if __name__ == "__main__":
    main(sys.argv[1:])
