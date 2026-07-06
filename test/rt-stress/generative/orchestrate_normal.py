#!/usr/bin/env python3
"""Normal (real-parallel) stress orchestrator for the generative engine (main.pony).

Runs the engine under a normal, production-default ponyc -- the real
multi-threaded runtime, NOT `use=systematic_testing`. Many cores run at the same
instant, so this catches true-simultaneity bugs (atomic tearing, memory ordering,
real lock-free contention) the serialized systematic mode cannot. The cost: runs
are NOT reproducible -- two runs of the same seed differ -- so there is no
determinism oracle here (that is orchestrate_systematic.py's job). What survives:
the conservation invariant (the engine itself exits non-zero on
sent != received != expected), crash detection, and the liveness watchdog (here it
fires on no progress -- engine silence -- so a slow run that keeps advancing is not
false-failed). Because a normal-mode crash does NOT reproduce, the engine runs under
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

    cmake --preset debug
    cmake --build --preset debug
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
    this is where the cycle detector gets its stress coverage.

    Normal mode draws one of five workloads (draw_workload): `mesh` (the static M0
    mesh), `cyclic` (successive groups of strongly-connected actors that drop their
    external reference, so the cycle detector must reclaim them), `backpressure`
    (a star of producers flooding one consumer that explicitly applies/releases
    runtime backpressure, driving the UNDER_PRESSURE muting path), `iso` (the mesh
    topology handing a nested `iso` object graph hop-to-hop by `consume`, so the
    receiver acquires a foreign mutable subgraph), or `actorref` (the mesh topology
    carrying a fresh actor `tag` per chain, forwarded and dropped, driving ORCA's
    actor-reference acquire/release). A `mesh` run draws a small->large magnitude in
    chains/ttl (the magnitude buckets, reaching ~1.16B messages); a `cyclic` run draws
    the magnitude in GENERATIONS and keeps chains/ttl small
    (total = generations*chains*(ttl+1)); a `backpressure` run draws the magnitude in
    MESSAGES per producer (total = producers*messages) and has no chains/ttl; an
    `iso` run reuses pingers and draws its graph shape (node-size/depth/breadth) plus
    small chains/ttl from its own buckets, then clamps chains to the acquire budget
    (the injection burst, not raw volume, governs its memory); an `actorref` run reuses
    pingers and draws chains from a magnitude bucket (RSS tracks chains) with a SMALL
    ttl (bounds run time; ttl >= 1 required). The payload (kind/size/mode) is drawn
    freely; a `mesh` run's ttl is then trimmed to a run-time ceiling (clamp_ttl), since
    a forward run's cost grows with chains^2 and a high-chains/high-ttl draw would
    otherwise run for hours; `iso` and `actorref` runs draw then ignore the payload,
    since their cargo is not a val payload.

    This mode is non-reproducible (real parallelism), so a seed maps to a
    *configuration* deterministically but not to an *execution*. The draw order is
    pinned (orchestrate_normal_test.py) so a seed yields a stable config:
    draw_workload (kind + cyclic shape + backpressure shape + iso cargo shape, fixed
    consumption; actorref adds no cargo draw) -> pingers (always drawn; used by a
    mesh/iso/actorref run) -> chains (bucketed, the kind's own buckets) -> ttl
    (bucketed) -> payload (kind, mode, size) -> ponynoblock -> the four shared swarm
    knobs -> ponymaxthreads last. Drawing the workload kind first lets the shared
    chains/ttl/payload draws pick the kind's bucket ranges while making the same
    sequence of draws regardless of kind. This contract is independent of the
    systematic driver's, and it intentionally differs from the four-kind draw, so
    historical normal-mode seeds remap.
    """
    program_seed = common.derive_seed(master_seed, "program")
    rng = random.Random(master_seed)

    (kind, generations, group, producers, messages, apply_every, node_size,
     depth, breadth) = common.draw_workload(rng)
    pingers = rng.choice(common.NORMAL_PINGERS)
    # chains/ttl come from the kind's own buckets; mesh/iso use the magnitude/iso
    # ranges, cyclic uses its small ranges, and backpressure draws-then-ignores
    # them. All kinds make the same two draw_bucketed calls (each exactly two rng
    # draws), so the kind picks the bucket ranges (the values) without changing the
    # draw sequence -- the fixed-consumption contract.
    if kind == "cyclic":
        chains_buckets = common.CYCLIC_CHAINS_BUCKETS
        ttl_buckets = common.CYCLIC_TTL_BUCKETS
    elif kind == "iso":
        chains_buckets = common.ISO_CHAINS_BUCKETS
        ttl_buckets = common.ISO_TTL_BUCKETS
    elif kind == "actorref":
        # actorref's memory tracks chains (flat in ttl), so it uses a chains-magnitude
        # bucket capped for RSS and a SMALL ttl bucket (ttl bounds run time via the hop
        # count and must be >= 1 -- a zero-hop (ttl=0) chain drives no acquire). It
        # must NOT fall into the mesh `else`: NORMAL_SIZE_BUCKETS for ttl would give
        # ttl up to 34000, exploding chains*ttl total messages and run time.
        chains_buckets = common.ACTORREF_CHAINS_BUCKETS
        ttl_buckets = common.ACTORREF_TTL_BUCKETS
    else:                          # mesh; backpressure draws-then-ignores
        chains_buckets = common.NORMAL_SIZE_BUCKETS
        ttl_buckets = common.NORMAL_SIZE_BUCKETS
    chains = common.draw_bucketed(rng, chains_buckets)
    ttl = common.draw_bucketed(rng, ttl_buckets)
    if kind == "iso":
        # Clamp chains to the safe acquire-flood budget for the drawn graph shape:
        # bigger graphs acquire more objects per hop, so they get fewer chains,
        # keeping chains * ttl * node_count <= ISO_ACQUIRE_BUDGET (the memory bound).
        # The chains draw above still happens unchanged (fixed consumption); this
        # only caps the value -- like clamp_ttl below trims a mesh run's ttl, no rng.
        chains = min(chains, common.iso_chains_cap(depth, breadth))
    payload, size, mode = common.draw_payload(rng)
    # Mesh run time is bounded by trimming ttl to fit the per-run ceiling (a
    # forward run's cost grows with chains^2, so a high-chains/high-ttl draw would
    # take hours). The clamp consumes no rng, so only an over-budget mesh ttl
    # changes -- cyclic/backpressure/iso draws are untouched. Cyclic keeps its own
    # tiny ttl range, backpressure ignores ttl, and iso draws from its own small ttl
    # buckets and caps chains above, so none needs the mesh clamp.
    if kind == "mesh":
        ttl = common.clamp_ttl(chains, ttl, mode, payload, size)

    # Emit only the shape fields the kind uses, so a contradictory field is never
    # passed to the engine: mesh has pingers+chains+ttl, cyclic has
    # generations+group+chains+ttl, backpressure has producers+messages+apply-every
    # (and NO chains/ttl), iso has pingers+chains+ttl+node-size/depth/breadth and its
    # OWN cargo knobs INSTEAD of the val payload. They live on the variants, not the
    # shared _Config.
    workload = {"seed": program_seed, "workload": kind}
    if kind == "cyclic":
        workload["generations"] = generations
        workload["group"] = group
        workload["chains"] = chains
        workload["ttl"] = ttl
    elif kind == "backpressure":
        workload["producers"] = producers
        workload["messages"] = messages
        workload["apply-every"] = apply_every
    elif kind == "iso":
        workload["pingers"] = pingers
        workload["chains"] = chains
        workload["ttl"] = ttl
        workload["node-size"] = node_size
        workload["node-depth"] = depth
        workload["node-breadth"] = breadth
    elif kind == "actorref":
        # actorref carries the mesh shape (pingers/chains/ttl) but NO payload -- its
        # cargo is a fresh actor tag per chain, built in the engine.
        workload["pingers"] = pingers
        workload["chains"] = chains
        workload["ttl"] = ttl
    else:
        workload["pingers"] = pingers
        workload["chains"] = chains
        workload["ttl"] = ttl
    # The val payload is emitted for every kind EXCEPT iso and actorref: those carry
    # their own cargo (iso's node graph; actorref's fresh actor tag), so the engine
    # never receives a payload flag it would ignore. The draw still happened (fixed
    # consumption); only the emit is per-kind.
    if kind not in ("iso", "actorref"):
        workload["payload"] = payload
        workload["payload-size"] = size
        workload["payload-mode"] = mode

    runtime = {}
    if rng.random() < 0.5:
        runtime["ponynoblock"] = True
    common.draw_swarm_knobs(rng, runtime)
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
                        default=common.DEFAULT_NORMAL_TIMEOUT_SECONDS,
                        help="absolute per-run backstop, seconds (default %d). A "
                        "hang is caught sooner by the no-progress watchdog (%ds of "
                        "silence); this only bounds a run whose real time runs far "
                        "past its estimate, so it can't eat the whole CI job."
                        % (common.DEFAULT_NORMAL_TIMEOUT_SECONDS,
                           common.DEFAULT_NORMAL_NO_PROGRESS_SECONDS))
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
    # The watchdog fires on NO PROGRESS (a hang), not on total length: the engine
    # heartbeats as it advances, so a slow-but-progressing run finishes instead of
    # false-failing. `t` (the per-run timeout) is the absolute backstop.
    def runner(b, c, t, m):
        return common.run_under_lldb(
            b, c, args.lldb, t, m,
            no_progress_seconds=common.DEFAULT_NORMAL_NO_PROGRESS_SECONDS)

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
