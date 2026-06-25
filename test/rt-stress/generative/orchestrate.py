#!/usr/bin/env python3
"""Generative + swarm stress orchestrator for the Pony runtime (M0).

This is the harness for test/rt-stress/generative/main.pony. It owns everything
about *how to run* the workload; the Pony program owns *what the workload does*
and holds no configuration logic. The split keeps every run a function of a
master seed (and the host's core count), so any failure replays from one number
on the same host.

From a master seed it:
  1. derives two seeds -- the program's RNG (`--seed`) and the scheduler
     interleaving (`--ponysystematictestingseed`, forced >= 1);
  2. draws a swarm configuration -- workload shape plus a random subset of the
     non-timing runtime knobs, always with `--ponynoscale` (the Wave-0
     reproducibility constraint) and `--ponymaxthreads` across [1, the host's
     probed physical core count];
  3. compiles the program once with a provided `--ponyc` (the binary is
     invariant across seeds -- all configuration is CLI/runtime);
  4. runs each seed TWICE under a wall-clock watchdog and an address-space cap
     so a deadlock or a pathological draw can't hang or OOM the host;
  5. requires the two runs' observable ORDER_SIG to match (the determinism
     oracle, always on -- a divergence is a race), decides pass/fail from that
     plus exit code + timeout, and writes a failure bundle. A seed replays from
     its number.

The ponyc passed in must be a debug + systematic build:

    make configure config=debug use=scheduler_scaling_pthreads,systematic_testing
    make build config=debug
    python3 test/rt-stress/generative/orchestrate.py \\
      --ponyc build/debug-scheduler_scaling_pthreads-systematic_testing/ponyc \\
      --use-flags scheduler_scaling_pthreads,systematic_testing \\
      --count 50 --out ~/tmp/rt-stress-out

`--version` does not echo the build's `use=` flags (a systematic build still
reports a bare `[debug]`), so the bundle records `--use-flags` separately.

The pure pieces (derive_seeds / resolve_config / build_argv / parse_result) are
unit-tested in orchestrate_test.py.
"""
import argparse
import hashlib
import json
import os
import platform
import random
import re
import resource
import shlex
import shutil
import subprocess
import sys

# The program name passed to `ponyc -b`, and the source package this script
# sits in (compile its own directory).
BINARY_NAME = "generative"
SOURCE_DIR = os.path.dirname(os.path.abspath(__file__))
# Repo root is four levels up: test/rt-stress/generative/orchestrate.py. If this
# script moves to a different depth, update the count (orchestrate_test.py pins
# that packages/ resolves from here).
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(SOURCE_DIR)))

U64_MASK = (1 << 64) - 1
DEFAULT_TIMEOUT_SECONDS = 120
DEFAULT_MEM_LIMIT_MB = 4096


def info(message):
    print(message, flush=True)


def die(message):
    print("FATAL: " + message, file=sys.stderr, flush=True)
    sys.exit(1)


def derive_seeds(master_seed):
    """Derive (program_seed, systematic_seed) from a master seed.

    Deterministic and stdlib-only. Both land in the U64 range. The program seed
    is forced >= 1 so the engine's `Rand(seed, 0)` can't hit the all-zero
    xoroshiro state, and the systematic seed >= 1 because the runtime rejects 0.
    """
    def mixed(label):
        digest = hashlib.sha256(("%d:%s" % (master_seed, label)).encode()).digest()
        return int.from_bytes(digest[:8], "big") & U64_MASK

    program_seed = mixed("program") or 1
    systematic_seed = mixed("systematic") or 1
    return program_seed, systematic_seed


def resolve_config(master_seed, max_threads):
    """Fully resolve a run from a master seed and the host's physical core count.

    Returns derived seeds, workload shape, and the runtime flag set. Every field
    is a pure function of master_seed alone EXCEPT `--ponymaxthreads`, which is
    drawn last across [1, max_threads] and is the only host-dependent value -- so
    --replay and the determinism check reconstruct an identical configuration on
    the same host (and an identical one bar the thread count on any host). Swarm
    testing: each non-timing runtime knob is independently present or absent
    (omission is the mechanism), and `--ponynoscale` is always on (Wave-0
    reproducibility constraint).

    The ORDER of the rng draws below is the seed-stability contract: a master
    seed maps to a config only as long as that draw sequence is unchanged. Add a
    new host-independent knob by appending its draw just BEFORE the final
    `ponymaxthreads` draw -- never insert mid-sequence (it remaps every seed and
    breaks historical --replay), and never after `ponymaxthreads` (its randint
    width is host-dependent, so a later draw would remap across hosts).
    orchestrate_test.py pins resolve_config(0, 8) to guard this.
    """
    program_seed, systematic_seed = derive_seeds(master_seed)
    rng = random.Random(master_seed)

    # These draws are a starting point, not tuned. The choice() knobs sample
    # every listed value equally, so their extremes are well covered; the
    # randint() ranges (chains, ttl) under-sample their edges (uniform rarely
    # draws 0/48 or 1/400). When this is tuned (M1, with allocation diversity),
    # bias toward important values where bugs cluster: edge-weight the ranges,
    # favor allocator size-class boundaries and occasional-large payload sizes,
    # and reach past the current max of 64 actors (contention/interleaving bugs
    # intensify with scale; the ASLR repro needed 32+).
    workload = {
        "seed": program_seed,
        "pingers": rng.choice([1, 2, 4, 8, 16, 32, 64]),
        "chains": rng.randint(1, 400),
        "ttl": rng.randint(0, 48),
        "payload": rng.choice(["string", "u64"]),
        "payload-size": rng.choice([1, 8, 64, 256, 1024, 4096]),
    }

    runtime = {
        "ponynoscale": True,
        "ponysystematictestingseed": systematic_seed,
    }
    if rng.random() < 0.5:
        runtime["ponynoblock"] = True
    if rng.random() < 0.5:
        runtime["ponygcinitial"] = rng.choice([1024, 1 << 16, 1 << 20])
    if rng.random() < 0.5:
        runtime["ponygcfactor"] = round(rng.choice([1.05, 1.5, 2.0, 4.0]), 2)
    if rng.random() < 0.5:
        runtime["ponycdinterval"] = rng.choice([10, 100, 1000])
    # Per-run allocation mode: `forward` re-sends one payload object down a chain
    # (shared-val refcounting), `fresh` allocates a new one at each hop
    # (allocation + collection churn).
    workload["payload-mode"] = rng.choice(["forward", "fresh"])

    # ponymaxthreads is drawn LAST and is the ONLY host-dependent field. Always
    # set, across [1, max_threads] so the swarm spans serial (1) through all
    # physical cores. The runtime REJECTS --ponymaxthreads > physical cores and
    # exits (src/libponyrt/sched/start.c; ponyint_cpu_count() collapses SMT
    # siblings, so "cores" is physical), so max_threads is the probed physical
    # count. It MUST stay last: randint() consumes a variable number of rng steps
    # (its width depends on max_threads), so any draw after it would remap across
    # hosts with different core counts. Last => every other field is a function
    # of master_seed alone (host-independent); only this one varies by host.
    runtime["ponymaxthreads"] = rng.randint(1, max_threads)

    return {"master_seed": master_seed, "workload": workload, "runtime": runtime}


def build_argv(binary, config):
    """Build the full command line for a resolved config.

    Workload args first, then runtime flags. A flag whose value is True is bare
    (`--ponynoscale`); anything else is `--key value`.
    """
    argv = [binary]
    for key, value in config["workload"].items():
        argv += ["--" + key, str(value)]
    for key, value in config["runtime"].items():
        if value is True:
            argv.append("--" + key)
        else:
            argv += ["--" + key, str(value)]
    return argv


def aslr_prefix():
    """Command prefix that disables ASLR for the run (Linux).

    Systematic-testing replay is reproducible only with address-space layout
    randomization off. The runtime orders some work by actor pointer values
    (hash_ptr-keyed GC maps iterated in bucket order), which ASLR randomizes per
    run -- a per-run input the `--ponysystematictestingseed` does not control.
    `setarch -R` (ADDR_NO_RANDOMIZE) removes it, restoring the
    same-seed-same-interleaving contract. Non-Linux hosts need their own
    equivalent; without one, replay and the determinism oracle are best-effort.
    """
    if (platform.system() == "Linux") and (shutil.which("setarch") is not None):
        return ["setarch", platform.machine(), "-R"]
    return []


def run_command(binary, config):
    """The full run command: the ASLR-disable prefix plus the engine argv."""
    return aslr_prefix() + build_argv(binary, config)


def parse_result(stdout):
    """Extract the engine's RECEIVED/SENT/EXPECTED/ORDER_SIG line, or {}."""
    result = {}
    for key in ("RECEIVED", "SENT", "EXPECTED", "ORDER_SIG"):
        match = re.search(key + r"=(\d+)", stdout)
        if match is not None:
            result[key.lower()] = match.group(1)
    return result


def run(cmd, **kwargs):
    info("+ " + " ".join(shlex.quote(c) for c in cmd))
    return subprocess.run(cmd, **kwargs)


def probe_max_threads(binary):
    """Probe the runtime's physical core count -- the ceiling `--ponymaxthreads`
    is checked against. Ask the engine for an impossible thread count and parse
    the count the runtime rejects with, so we match the runtime's own
    (SMT-collapsing) definition exactly rather than guessing from
    `os.cpu_count()`, which is logical and would over-count on an SMT host (and
    re-introduce the spurious-reject bug). Falls back to `os.cpu_count()` if the
    rejection can't be parsed.
    """
    cmd = aslr_prefix() + [binary, "--ponymaxthreads", "1000000",
                           "--ponynoscale", "--ponysystematictestingseed", "1"]
    try:
        result = subprocess.run(cmd, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=30)
        text = (result.stdout.decode(errors="replace")
                + result.stderr.decode(errors="replace"))
        match = re.search(r"> (\d+)\)", text)
        if match is not None:
            return int(match.group(1))
    except subprocess.TimeoutExpired:
        pass
    fallback = os.cpu_count() or 1
    info("note: could not probe physical cores from the runtime; using "
         "os.cpu_count()=%d" % fallback)
    return fallback


def ponyc_version(ponyc):
    """Capture `ponyc --version` verbatim and in full (it is the primary
    provenance; the embedded git SHA is authoritative even past a tree move)."""
    result = subprocess.run([ponyc, "--version"], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    return result.stdout.decode(errors="replace").strip()


def compile_engine(ponyc, out_dir):
    """Compile the engine once with the provided ponyc. stdlib resolves from
    REPO_ROOT/packages, matching .ci-scripts/systematic-testing."""
    env = dict(os.environ, PONYPATH=os.path.join(REPO_ROOT, "packages"))
    result = run([ponyc, "-d", "-b", BINARY_NAME, "--pic", "-o", out_dir,
                  SOURCE_DIR], env=env)
    if result.returncode != 0:
        die("compiling the engine failed")
    binary = os.path.join(out_dir, BINARY_NAME)
    if not os.access(binary, os.X_OK):
        die("engine binary not produced at " + binary)
    return binary


class RunResult:
    """How a single run ended: outcome, exit status, and captured streams."""

    def __init__(self, outcome, returncode, signal, stdout, stderr):
        self.outcome = outcome  # "pass" | "fail" | "timeout"
        self.returncode = returncode
        self.signal = signal
        self.stdout = stdout
        self.stderr = stderr


def run_once(binary, config, timeout, mem_limit_bytes):
    """Run the binary under a wall-clock watchdog and an RLIMIT_AS cap.

    A deterministic schedule can still deadlock, so the timeout is real; the
    address-space cap (== `ulimit -v`, covering the pool allocator's mmap'd
    regions) keeps a pathological draw from taking down the host. Near the cap
    the runtime hits allocation failure and aborts rather than exiting cleanly,
    so a signal is recorded for the bundle to make that legible.
    """
    argv = run_command(binary, config)

    def set_limits():
        resource.setrlimit(resource.RLIMIT_AS,
                           (mem_limit_bytes, mem_limit_bytes))

    info("+ " + " ".join(shlex.quote(a) for a in argv))
    try:
        completed = subprocess.run(argv, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, timeout=timeout,
                                   preexec_fn=set_limits)
    except subprocess.TimeoutExpired as expired:
        return RunResult("timeout", None, None,
                         _decode(expired.stdout), _decode(expired.stderr))

    returncode = completed.returncode
    signal = -returncode if returncode < 0 else None
    outcome = "pass" if returncode == 0 else "fail"
    return RunResult(outcome, returncode, signal,
                     completed.stdout.decode(errors="replace"),
                     completed.stderr.decode(errors="replace"))


def _decode(raw):
    return raw.decode(errors="replace") if raw else ""


def bundle_for(config, version, use_flags, argv, limits, result):
    """Assemble the failure-bundle dict: everything needed to reproduce and
    attribute a run from the record alone."""
    return {
        "master_seed": config["master_seed"],
        "program_seed": config["workload"]["seed"],
        "systematic_seed": config["runtime"]["ponysystematictestingseed"],
        "workload": config["workload"],
        "runtime_flags": config["runtime"],
        "cli": " ".join(shlex.quote(a) for a in argv),
        "ponyc_version": version,
        "use_flags": use_flags,
        "limits": limits,
        "outcome": result.outcome,
        "returncode": result.returncode,
        "signal": result.signal,
        "stdout": result.stdout,
        "stderr": result.stderr,
    }


def write_bundle(out_dir, bundle):
    path = os.path.join(out_dir, "bundle-%d.json" % bundle["master_seed"])
    with open(path, "w") as handle:
        json.dump(bundle, handle, indent=2, sort_keys=True)
    return path


def summary_line(config, result):
    parsed = parse_result(result.stdout)
    shape = config["workload"]
    detail = "received=%s expected=%s sig=%s" % (
        parsed.get("received", "?"), parsed.get("expected", "?"),
        parsed.get("order_sig", "?"))
    return "[seed %d] %s (pingers=%d chains=%d ttl=%d payload=%s) %s" % (
        config["master_seed"], result.outcome.upper(), shape["pingers"],
        shape["chains"], shape["ttl"], shape["payload"], detail)


def execute(binary, config, version, use_flags, out_dir, timeout,
            mem_limit_bytes):
    """Run one seed once; tee output, write a bundle on non-pass, return the
    RunResult."""
    result = run_once(binary, config, timeout, mem_limit_bytes)
    info(summary_line(config, result))
    if result.stdout:
        info(result.stdout.rstrip("\n"))
    if result.stderr:
        info(result.stderr.rstrip("\n"))
    if result.outcome != "pass":
        argv = run_command(binary, config)
        limits = {"timeout_seconds": timeout,
                  "mem_limit_bytes": mem_limit_bytes}
        path = write_bundle(out_dir,
                            bundle_for(config, version, use_flags, argv, limits,
                                       result))
        info("wrote failure bundle: " + path)
    return result


def check_determinism(binary, config, version, use_flags, out_dir, timeout,
                      mem_limit_bytes):
    """Run one seed twice and compare the observable ORDER_SIG. Divergence at a
    fixed seed pair (with ASLR disabled) is a race. A non-pass run is a failure
    in its own right. On any determinism failure a `determinism-<seed>.json`
    bundle is written capturing both runs, so the divergence -- the headline
    oracle failure, which otherwise leaves no on-disk artifact since both runs
    exit 0 -- reproduces from the record, not just from stdout."""
    seed = config["master_seed"]
    first = execute(binary, config, version, use_flags, out_dir, timeout,
                    mem_limit_bytes)
    second = execute(binary, config, version, use_flags, out_dir, timeout,
                     mem_limit_bytes)
    sig_a = parse_result(first.stdout).get("order_sig")
    sig_b = parse_result(second.stdout).get("order_sig")
    runs_ok = (first.outcome == "pass") and (second.outcome == "pass")
    have_sigs = (sig_a is not None) and (sig_b is not None)

    if runs_ok and have_sigs and (sig_a == sig_b):
        info("[seed %d] determinism ok: ORDER_SIG=%s reproduced" % (seed, sig_a))
        return True

    if not runs_ok:
        verdict = "a run did not pass (cannot compare ORDER_SIG)"
    elif not have_sigs:
        verdict = "a run exited 0 without an ORDER_SIG (cannot compare)"
    elif not aslr_prefix():
        # ASLR was not disabled, so a divergence is INCONCLUSIVE rather than a
        # race: systematic replay is address-dependent without ASLR off (the
        # runtime orders work by actor pointer values).
        verdict = "INCONCLUSIVE: ORDER_SIG %s vs %s but ASLR was not disabled" \
            % (sig_a, sig_b)
    else:
        verdict = "DETERMINISM FAILURE: %s != %s (a race)" % (sig_a, sig_b)
    info("[seed %d] %s" % (seed, verdict))

    # Build the top-level bundle from the failing run when one failed (otherwise
    # the headline outcome would describe a passing run); the determinism block
    # below carries both runs regardless.
    failed = first if first.outcome != "pass" else second
    bundle = bundle_for(config, version, use_flags, run_command(binary, config),
                        {"timeout_seconds": timeout,
                         "mem_limit_bytes": mem_limit_bytes}, failed)
    bundle["determinism"] = {"verdict": verdict, "sig_run1": sig_a,
                             "sig_run2": sig_b, "outcome_run1": first.outcome,
                             "outcome_run2": second.outcome}
    path = os.path.join(out_dir, "determinism-%d.json" % seed)
    with open(path, "w") as handle:
        json.dump(bundle, handle, indent=2, sort_keys=True)
    info("wrote determinism bundle: " + path)
    return False


def resolve_seeds(args):
    if args.master_seed is not None:
        return [args.master_seed]
    if args.replay is not None:
        return [args.replay]
    if args.count is not None:
        return list(range(args.start, args.start + args.count))
    try:
        return [int(token) for token in args.seeds.split(",")]
    except ValueError:
        die("bad --seeds (expected comma-separated integers): " + args.seeds)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ponyc", required=True,
                        help="path to a debug + systematic ponyc")
    parser.add_argument("--use-flags", required=True,
                        help="use= flags the ponyc was built with (provenance)")
    parser.add_argument("--out", help="output dir for the binary and bundles")
    parser.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT_SECONDS,
                        help="per-run wall-clock watchdog, seconds (default %d)"
                        % DEFAULT_TIMEOUT_SECONDS)
    parser.add_argument("--mem-limit-mb", type=int,
                        default=DEFAULT_MEM_LIMIT_MB,
                        help="per-run address-space cap, MB (default %d); a cap "
                        "below the runtime's startup need reports as a FAIL"
                        % DEFAULT_MEM_LIMIT_MB)
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
    if not os.access(ponyc, os.X_OK):
        die("ponyc not executable at " + ponyc)
    if "systematic_testing" not in args.use_flags:
        die("--use-flags must include systematic_testing: the engine passes "
            "--ponysystematictestingseed, which a non-systematic runtime rejects "
            "(every seed would 'fail')")

    out_dir = os.path.abspath(args.out) if args.out else os.path.join(
        REPO_ROOT, "rt-stress-out")
    os.makedirs(out_dir, exist_ok=True)
    mem_limit_bytes = args.mem_limit_mb * 1024 * 1024

    version = ponyc_version(ponyc)
    info("ponyc: " + version.replace("\n", " | "))
    info("use-flags: " + args.use_flags)
    if not aslr_prefix():
        info("note: setarch -R unavailable (non-Linux or no setarch) -- "
             "systematic replay needs ASLR disabled, so replay and the "
             "determinism oracle are best-effort here")
    binary = compile_engine(ponyc, out_dir)
    max_threads = probe_max_threads(binary)
    info("physical cores (--ponymaxthreads ceiling): %d" % max_threads)

    seeds = resolve_seeds(args)
    if not seeds:
        die("no seeds to run -- --count must be >= 1 (a 0/negative --count or "
            "empty --seeds runs nothing and would exit 0)")
    replay = args.replay is not None
    if replay:
        config = resolve_config(seeds[0], max_threads)
        info("replay config: " + json.dumps(config, sort_keys=True))
        info("replay cli: " + " ".join(
            shlex.quote(a) for a in run_command(binary, config)))

    failures = 0
    for seed in seeds:
        config = resolve_config(seed, max_threads)
        # Determinism is checked on every seed (always-on): each runs twice and
        # the observable ORDER_SIG must match. The second run is also a full
        # conservation/crash/liveness run, so it is not wasted.
        if not check_determinism(binary, config, version, args.use_flags,
                                 out_dir, args.timeout, mem_limit_bytes):
            failures += 1

    info("ran %d seed(s), %d failure(s)" % (len(seeds), failures))
    if failures != 0:
        sys.exit(1)


if __name__ == "__main__":
    main(sys.argv[1:])
