#!/usr/bin/env python3
"""Shared, mode-agnostic mechanism for the generative stress orchestrators.

Both orchestrators drive the same engine (main.pony) and share everything here:
seed derivation, the workload draws, the command line, the run watchdog, result
parsing, and the failure bundle.

  - orchestrate_systematic.py -- serialized, reproducible: runs the engine under
    `use=systematic_testing`, exploring scheduler interleavings deterministically.
  - orchestrate_normal.py -- the real, multi-threaded runtime: true parallelism,
    non-reproducible, catching the simultaneity bugs the systematic mode can't.

What DIFFERS between the two modes lives in the two drivers, NOT behind a flag in
here. This file holds only what is byte-identical to both, so neither mode reads
an `if systematic` branch in shared code.

Cross-platform: this runs on Linux, macOS, AND Windows. Both modes target all
three TIER1 platforms (on Windows the systematic build uses
`use=systematic_testing` alone -- Windows scales the scheduler with native
primitives, not pthreads). The RLIMIT_AS memory cap is applied only where the OS
honors it (Linux): Windows lacks the `resource` module and macOS rejects the
limit, so both fall back to host OOM handling. The wall-clock watchdog is
portable.

The pure pieces (derive_seed / resolve_systematic_workload / draw_* / build_argv /
run_command / parse_result / lldb_argv / lldb_exit_code) are unit-tested in
stress_common_test.py; the run classifiers (run_once / run_under_lldb) are tested
by injecting a fake _capture.
"""
import hashlib
import json
import os
import re
import shlex
import signal
import subprocess
import sys

try:
    import resource  # POSIX only; absent on Windows
except ImportError:
    resource = None

# The program name passed to `ponyc -b`, and the source package the orchestrators
# sit in (they compile their own directory).
BINARY_NAME = "generative"
SOURCE_DIR = os.path.dirname(os.path.abspath(__file__))
# Repo root is four levels up: test/rt-stress/generative/<script>.py. If these
# scripts move to a different depth, update the count (stress_common_test.py pins
# that packages/ resolves from here).
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(SOURCE_DIR)))

U64_MASK = (1 << 64) - 1
DEFAULT_TIMEOUT_SECONDS = 120
# Normal mode's per-run watchdog: a flat ~100 min, ~2x the longest plausible large
# run with slow-CI margin (a hung run, not its true length, is the worst case). Big
# enough that host-speed variance never false-positives a legitimate run, so the
# bucket and size-table numbers can stay hardcoded with no startup calibration. Systematic
# keeps DEFAULT_TIMEOUT_SECONDS -- its runs are seconds (the engine now reaches natural
# quiescence through full shutdown rather than forcing an exit, so a large drawn config
# can take ~10s, still well inside the 120s cap).
DEFAULT_NORMAL_TIMEOUT_SECONDS = 6000
DEFAULT_MEM_LIMIT_MB = 4096


def info(message):
    print(message, flush=True)


def die(message):
    print("FATAL: " + message, file=sys.stderr, flush=True)
    sys.exit(1)


def derive_seed(master_seed, label):
    """Deterministic, stdlib-only U64 seed from (master_seed, label), floored to
    >= 1. The floor is load-bearing: the runtime rejects systematic seed 0, and
    the engine's `Rand(seed, 0)` hits the degenerate all-zero xoroshiro state at
    0. Different labels yield independent seeds from the same master."""
    digest = hashlib.sha256(("%d:%s" % (master_seed, label)).encode()).digest()
    return (int.from_bytes(digest[:8], "big") & U64_MASK) or 1


def resolve_systematic_workload(rng, program_seed):
    """The systematic-mode workload shape: the program seed (passed, not drawn) plus
    the five swarm-drawn fields, in the dict-insertion and rng-draw order that is the
    seed-stability contract.

    Normal mode does NOT use this -- it composes its own bucketed draw plus a
    payload whose string size is limited by the message count (draw_bucketed /
    draw_payload); see orchestrate_normal.resolve_config. Systematic stays small and
    fixed-range on purpose: it serializes the scheduler, so a billion-message run's
    interleaving space is intractable.

    `payload-mode` is deliberately NOT here -- the systematic driver draws it via
    draw_payload_mode() AFTER its runtime-knob draws and BEFORE draw_max_threads().
    The interleaving of these draws in resolve_config is the contract: move any draw
    and every seed remaps (breaking historical --replay). resolve_config(0, 8) is
    pinned in orchestrate_systematic_test.py to guard it.
    """
    return {
        "seed": program_seed,
        "pingers": rng.choice([1, 2, 4, 8, 16, 32, 64]),
        "chains": rng.randint(1, 400),
        "ttl": rng.randint(0, 48),
        "payload": rng.choice(["string", "u64"]),
        "payload-size": rng.choice([1, 8, 64, 256, 1024, 4096]),
    }


def draw_swarm_knobs(rng, runtime):
    """Draw the four optional runtime knobs shared by both modes, adding the
    present ones to `runtime`. Omission IS the swarm mechanism -- each knob is
    independently present or absent. Draw order is part of the seed-stability
    contract; do not reorder.

    `ponynoblock` is deliberately NOT here: systematic forces it on (the
    determinism oracle needs the cycle detector off), normal draws it as its own
    swarm knob -- so each driver handles ponynoblock itself, and only these four
    are common.
    """
    if rng.random() < 0.5:
        runtime["ponynoscale"] = True
    if rng.random() < 0.5:
        # --ponygcinitial is a base-2 EXPONENT, not a byte count: it defers an
        # actor's GC until it is using 2^N bytes (runtime default N=14 = 16 KiB).
        # These draw 1 KiB / 64 KiB / 1 MiB thresholds (a spread around the
        # default). Pass exponents, NOT byte counts -- the runtime computes
        # `1 << N`, so a byte count like 65536 is masked (mod 64) to a 1-byte
        # threshold on x86-64, silently forcing GC on nearly every allocation.
        runtime["ponygcinitial"] = rng.choice([10, 16, 20])
    if rng.random() < 0.5:
        runtime["ponygcfactor"] = round(rng.choice([1.05, 1.5, 2.0, 4.0]), 2)
    if rng.random() < 0.5:
        runtime["ponycdinterval"] = rng.choice([10, 100, 1000])


def draw_payload_mode(rng):
    """`forward` re-sends one payload object down a chain (shared-val
    refcounting); `fresh` allocates a new one at each hop (allocation + collection
    churn). Used only by the SYSTEMATIC driver now -- normal mode draws kind, size,
    and mode together in draw_payload. COUPLING: in the systematic driver this
    must be drawn AFTER the runtime-knob draws and BEFORE draw_max_threads() -- that
    position is its seed-stability contract; reordering it remaps every systematic
    seed."""
    return rng.choice(["forward", "fresh"])


def draw_max_threads(rng, max_threads):
    """`--ponymaxthreads`, drawn LAST and the only host-dependent field, across
    [1, max_threads] so the swarm spans serial (1) through all physical cores. It
    MUST stay last: randint() consumes a variable number of rng steps (its width
    depends on max_threads), so any draw after it would remap across hosts with
    different core counts. The runtime REJECTS --ponymaxthreads > physical cores,
    so max_threads is the probed physical count (see probe_max_threads)."""
    return rng.randint(1, max_threads)


# Normal-mode magnitude buckets. Each of chains and ttl draws a bucket
# (small/medium/large at 25/50/25), then a uniform value within it. Ranges are
# (lo, hi) inclusive and tuned on the calibration host (WSL2, 2 threads, debug,
# ~1M msg/s at scale) so same-bucket pairs land near the time yardstick: S*S
# ~secs-3min, M*M ~3-12min, L*L ~12-19min (top ~1.16B messages ~ 15-20 min).
# chains and ttl share these ranges. Systematic mode does NOT use them -- it stays
# small/fixed-range (see resolve_systematic_workload). Locked, not provisional.
NORMAL_SIZE_BUCKETS = {
    "small": (1500, 14000),
    "medium": (14001, 27000),
    "large": (27001, 34000),
}

# String-payload sizes grouped by per-hop cost into small/medium/large tables. A
# fresh string's run cost is ~ messages * per-hop-cost(size), so the costlier tables
# are offered only to smaller runs: each table's third field is the most messages a
# fresh run of its slowest size keeps within the ~30-min soak budget. The 4096/1024/
# 256 figures are measured at scale on the calibration host (the per-message rate
# decays as a run grows, so these are not short-run extrapolations); the small table's
# cap is from the same decay curve, which reproduces the measured ones within ~3%, and
# the 100-min per-run hard timeout backstops any drift. draw_payload limits a fresh
# string to the sizes whose table fits the run's message count -- a big run can only
# draw a cheap size, a small run any. Forward and u64 payloads allocate once (or
# never), so their cost is ~flat in size and they draw from every size. COUPLING: the
# per-table caps are a property of the engine's per-hop cost -- if that changes (e.g. a
# payload-integrity check, per the README's deferred work), re-measure them.
STRING_SIZE_TABLES = (
    # (name, sizes, max fresh-string messages within the ~30-min budget)
    ("small",  [1, 8, 64],  990_000_000),
    ("medium", [256, 1024], 520_000_000),
    ("large",  [4096],      300_000_000),
)
ALL_STRING_SIZES = [s for _name, _sizes, _cap in STRING_SIZE_TABLES for s in _sizes]

# Normal-mode actor counts. pingers=1 -- a lone actor that only ever forwards to
# itself -- is included on purpose: it exercises the ORCA self-send path. That path
# used to flood the forwarded object's owner with acquire messages and balloon memory
# (~4.5 GB at ~7.7M messages), so this draw once excluded pingers=1. But that was a
# real runtime defect, fixed in PR #5594: the in-queue self-sent object is pinned so
# the per-actor sweep no longer releases and re-borrows it every hop. With the fix a
# pingers=1 run is bounded (~40 MB even at the 34000-chain max, conserving), so
# drawing it here keeps a standing stress on that fix -- a regression would reappear
# as the same blowup.
NORMAL_PINGERS = [1, 2, 4, 8, 16, 32, 64]

# Normal-mode cyclic-garbage draws (the `cyclic` workload). A run draws this
# workload with probability CYCLIC_PROBABILITY; otherwise it is a `mesh` run. The
# cyclic workload spawns `generations` successive groups of `group` actors, each
# group a strongly connected reference cycle the detector must reclaim.
#
# Memory bound (calibrated on the engine, CD-OFF worst case -- the leak case, since
# normal draws ponynoblock ~50%): the Churner front-loads generations with no
# backpressure, so peak live actors ~= generations*group, at ~5.5 KB/worker, and
# group is SUPER-LINEAR (a group of g is g actors each holding a g-element tag
# array). The worst drawn case -- generations 8000 * group 16 = 128k workers, with
# the largest cyclic chains/ttl and a fresh 4096-byte string -- measures ~1.2 GB
# peak RSS and fits under the 4 GB DEFAULT_MEM_LIMIT_MB with ~3x margin, CD on and
# off. group 32 is deliberately excluded: same worker count costs ~3x the memory.
#
# Time bound: total messages = generations * chains * (ttl+1), so cyclic chains/ttl
# are kept SMALL (their own buckets below, NOT the mesh's magnitude buckets, which
# would explode the count) -- max ~1.1M messages, trivially within the soak budget.
# Small chains/ttl also let a generation drain and be collected fast, so collection
# races creation rather than piling up. COUPLING: these bounds are a property of the
# engine's per-worker footprint and the front-loading Churner -- re-measure if the
# cyclic actor's fields or the Churner's pacing change.
CYCLIC_PROBABILITY = 0.5
CYCLIC_GROUPS = [2, 4, 8, 16]
CYCLIC_GENERATION_BUCKETS = {
    "small": (10, 800),
    "medium": (801, 3000),
    "large": (3001, 8000),
}
CYCLIC_CHAINS_BUCKETS = {"small": (1, 2), "medium": (3, 5), "large": (6, 8)}
CYCLIC_TTL_BUCKETS = {"small": (1, 4), "medium": (5, 10), "large": (11, 16)}


def draw_bucketed(rng, buckets):
    """Draw a small/medium/large bucket at 25/50/25, then a uniform value within it.
    Exactly two rng draws (the bucket roll, then the value), and that order is part
    of the seed-stability contract. `buckets` is a name->(lo, hi) inclusive dict with
    small/medium/large keys."""
    roll = rng.random()
    if roll < 0.25:
        lo, hi = buckets["small"]
    elif roll < 0.75:
        lo, hi = buckets["medium"]
    else:
        lo, hi = buckets["large"]
    return rng.randint(lo, hi)


def draw_payload(rng, msgs):
    """Draw payload kind, size, and mode. Returns (payload, size, mode).

    The available string SIZE is limited by `msgs`: a fresh string's cost grows with
    both message count and size, so a big run may draw only a cheap (small) size while
    a small run may draw any (see STRING_SIZE_TABLES). Forward and u64 payloads are
    flat-cost in size and draw from every size. Runs so large that even the small
    table's budget is exceeded still draw from it (a slight, hard-timeout-bounded
    overrun) rather than being denied a string.

    Always exactly three rng draws -- kind, mode, size -- in that fixed order, so the
    draw stream is stable: the size table changes which value the third draw lands on,
    never how much rng it consumes."""
    kind = rng.choice(["string", "u64"])
    mode = rng.choice(["forward", "fresh"])
    if kind == "string" and mode == "fresh":
        sizes = [s for _n, table, cap in STRING_SIZE_TABLES if msgs <= cap
                 for s in table]
        if not sizes:                      # past even the small table's budget;
            sizes = STRING_SIZE_TABLES[0][1]  # still allow the cheapest sizes
    else:
        sizes = ALL_STRING_SIZES
    size = sizes[int(rng.random() * len(sizes))]
    return kind, size, mode


def draw_cyclic(rng):
    """Draw the workload kind plus the cyclic-only shape, returning
    (workload, generations, group). ALWAYS the same fixed sequence of four logical
    draws -- the kind roll, the bucketed generations (a roll + a value), and the
    group choice -- with no branch on the rolled kind, so the kind changes which
    keys the config later emits but not how many draws this makes (mirrors
    draw_payload's fixed-consumption discipline; "logical draws" because a randint's
    underlying bit consumption varies with its value, but the call sequence does
    not). generations and group are drawn even for a mesh run (and then ignored) to
    keep that sequence fixed."""
    workload = "cyclic" if rng.random() < CYCLIC_PROBABILITY else "mesh"
    generations = draw_bucketed(rng, CYCLIC_GENERATION_BUCKETS)
    group = rng.choice(CYCLIC_GROUPS)
    return workload, generations, group


def build_argv(binary, config):
    """Full command line for a resolved config: workload args first, then runtime
    flags. A flag whose value is True is bare (`--ponynoscale`); anything else is
    `--key value`."""
    argv = [binary]
    for key, value in config["workload"].items():
        argv += ["--" + key, str(value)]
    for key, value in config["runtime"].items():
        if value is True:
            argv.append("--" + key)
        else:
            argv += ["--" + key, str(value)]
    return argv


def run_command(binary, config):
    """The full run command for a resolved config. There is deliberately no
    ASLR/setarch wrapper: systematic replay is layout-independent since
    #5566/#5570, and the normal mode is non-reproducible by design, so this is
    exactly the engine argv on every platform."""
    return build_argv(binary, config)


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


def probe_max_threads(binary, seed_args):
    """Probe the runtime's physical core count -- the ceiling `--ponymaxthreads`
    is checked against. Ask the engine for an impossible thread count and parse
    the count the runtime rejects with, so we match the runtime's own
    (SMT-collapsing) definition exactly rather than guessing from os.cpu_count(),
    which is logical and would over-count on an SMT host.

    `seed_args` carries the mode-specific flags the runtime needs to start: the
    systematic driver passes `--ponysystematictestingseed 1` (its runtime requires
    a seed); the normal driver passes none (a normal runtime rejects that flag).
    Falls back to os.cpu_count() if the rejection can't be parsed -- e.g. on a
    platform whose reject message differs (Windows is unverified; the fallback
    covers it)."""
    cmd = [binary, "--ponymaxthreads", "1000000", "--ponynoscale"] + list(seed_args)
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


def is_systematic_build(binary):
    """True if the compiled engine is a `use=systematic_testing` build.

    Guards a real footgun for the normal driver: point it at a systematic ponyc
    and -- with no `--ponysystematictestingseed` -- the systematic runtime runs
    SERIALIZED off a wall-clock seed, silently defeating real parallelism while
    conservation still passes and CI stays green. A systematic runtime prints a
    "Systematic testing using seed" banner at startup, so run a trivial workload
    and look for it. A normal runtime just runs the workload and exits."""
    try:
        result = subprocess.run(
            [binary, "--pingers", "1", "--chains", "1", "--ttl", "0",
             "--ponynoscale"], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            timeout=30)
    except subprocess.TimeoutExpired:
        # A trivial ttl=0 run should finish instantly; if it somehow hangs we
        # can't classify, so don't block startup -- the per-seed watchdog catches
        # a genuinely hung engine. (Mirrors probe_max_threads' graceful fallback.)
        info("note: could not probe build type (engine did not finish); "
             "assuming normal")
        return False
    text = (result.stdout.decode(errors="replace")
            + result.stderr.decode(errors="replace"))
    return "Systematic testing using seed" in text


def compile_engine(ponyc, out_dir):
    """Compile the engine once with the provided ponyc (always `-d`). stdlib
    resolves from REPO_ROOT/packages. The produced binary is `generative` on POSIX
    and `generative.exe` on Windows; `os.X_OK` is meaningless on Windows, so
    existence (os.path.isfile) is the cross-platform check."""
    env = dict(os.environ, PONYPATH=os.path.join(REPO_ROOT, "packages"))
    result = run([ponyc, "-d", "-b", BINARY_NAME, "--pic", "-o", out_dir,
                  SOURCE_DIR], env=env)
    if result.returncode != 0:
        die("compiling the engine failed")
    binary = os.path.join(out_dir, BINARY_NAME)
    if os.name == "nt":
        binary += ".exe"
    if not os.path.isfile(binary):
        die("engine binary not produced at " + binary)
    return binary


class RunResult:
    """How a single run ended: outcome, exit status, and captured streams.

    `signal` is a diagnostic, recorded for the bundle and never parsed back, so
    its type intentionally differs by runner: the direct runner (run_once) sets a
    terminating signal NUMBER (int) or None; the lldb runner (run_under_lldb) sets
    the signal NAME (str, e.g. "SIGSEGV") or "crash" when lldb reports no number.
    """

    def __init__(self, outcome, returncode, signal, stdout, stderr):
        self.outcome = outcome  # "pass" | "fail" | "timeout"
        self.returncode = returncode
        self.signal = signal
        self.stdout = stdout
        self.stderr = stderr


def _rlimit_as_supported(platform, resource_available):
    """Whether to install the RLIMIT_AS address-space cap on this platform.

    An allowlist, not a denylist: the cap is applied only on Linux, the one
    TIER1 platform validated to honor RLIMIT_AS. Windows lacks the POSIX
    `resource` module entirely; macOS HAS it but Darwin does not honor
    RLIMIT_AS, and `setrlimit` raises there -- inside `preexec_fn` (post-fork,
    pre-exec) that aborts the spawn with `SubprocessError`, crashing every run
    before the engine starts rather than capping anything.

    Allowlisting is deliberate: that macOS crash is the failure mode of applying
    the cap on an unvalidated platform, so any platform we have not confirmed
    falls back to the host's OOM handling (as Windows already did) instead of
    risking the same crash. A new platform that honors RLIMIT_AS is added here
    explicitly once validated.

    Pure (platform + resource-availability in, bool out) so it is testable from
    any host regardless of where the suite runs."""
    return resource_available and (platform == "linux")


def _capture(argv, timeout, mem_limit_bytes):
    """Run argv under a wall-clock watchdog and (on Linux) an RLIMIT_AS cap;
    return (timed_out, returncode, stdout, stderr). Mechanism only -- the caller
    classifies the outcome.

    The timeout is the primary guardrail on every platform -- a real-parallel or
    deterministic run can still deadlock. On timeout we kill the whole process
    GROUP, not just the direct child: the normal mode's direct child is lldb and
    the engine is its grandchild, so SIGKILLing only lldb would orphan a hung
    engine (it reparents to init and keeps consuming the host across the soak).
    `start_new_session` puts the child in its own group so killpg reaps the tree.

    The address-space cap (== `ulimit -v`, covering the pool allocator's mmap'd
    regions) is applied only where the OS honors it -- see _rlimit_as_supported
    for why Windows and macOS are excluded; there preexec_fn stays None and the
    job relies on the host's OOM handling."""
    preexec = None
    if (mem_limit_bytes is not None) and _rlimit_as_supported(
            sys.platform, resource is not None):
        def set_limits():
            resource.setrlimit(resource.RLIMIT_AS,
                               (mem_limit_bytes, mem_limit_bytes))
        preexec = set_limits

    info("+ " + " ".join(shlex.quote(a) for a in argv))
    proc = subprocess.Popen(argv, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, preexec_fn=preexec,
                            start_new_session=(os.name == "posix"))
    try:
        stdout, stderr = proc.communicate(timeout=timeout)
        return (False, proc.returncode, _decode(stdout), _decode(stderr))
    except subprocess.TimeoutExpired:
        _kill_process_tree(proc)
        stdout, stderr = proc.communicate()
        return (True, None, _decode(stdout), _decode(stderr))


def _kill_process_tree(proc):
    """Kill proc and, on POSIX, its whole session group -- so a hung lldb does not
    orphan the engine grandchild. Windows has no killpg, so it kills the direct
    child and leans on the job's container teardown for stragglers."""
    if os.name == "posix":
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            return
        except (ProcessLookupError, PermissionError):
            pass
    proc.kill()


def run_once(binary, config, timeout, mem_limit_bytes):
    """Direct run (no debugger): classify the outcome straight from the process
    exit code. A negative code is a terminating signal. Used by the systematic
    driver, whose runs reproduce -- a crash is re-run under a debugger locally
    from its seed. (The normal driver uses run_under_lldb instead; see there.)"""
    timed_out, returncode, stdout, stderr = _capture(
        run_command(binary, config), timeout, mem_limit_bytes)
    if timed_out:
        return RunResult("timeout", None, None, stdout, stderr)
    signal = -returncode if returncode < 0 else None
    outcome = "pass" if returncode == 0 else "fail"
    return RunResult(outcome, returncode, signal, stdout, stderr)


def lldb_argv(lldb, engine_argv):
    """Wrap the engine argv to run under `lldb --batch`. On a crash the on-crash
    commands (`frame variable`, `bt all`) print the crash site to the captured
    output and `quit 1` makes lldb exit non-zero, so the backtrace survives even
    when the run doesn't reproduce. POSIX is the careful path: the Pony runtime
    uses SIGUSR2 for scheduler wakeups (and SIGINT for shutdown), so lldb must pass
    those through WITHOUT stopping -- otherwise it halts on every scheduler signal
    and the run hangs. We stop at `main`, configure that pass-through, then
    continue. Windows has no such signals, so it just runs. Mirrors the
    `usedebugger=lldb` invocation in the Makefile / make.ps1.

    `register read` is deliberately omitted: we don't upload the failing binary,
    so raw register addresses can't be re-symbolized -- `frame variable` + `bt all`
    (symbolized at capture in a debug build) are the useful artifacts.

    COUPLING: the pass-through list (SIGINT, SIGUSR2) is complete only for today's
    socketless engine. If the engine ever opens a socket the runtime arms SIGPIPE,
    and lldb would stop on it -- pass it through here too, or the run hangs."""
    on_crash = ["--one-line-on-crash", "frame variable",
                "--one-line-on-crash", "bt all",
                "--one-line-on-crash", "quit 1"]
    if os.name == "posix":
        setup = ["--one-line", "breakpoint set --name main",
                 "--one-line", "run",
                 "--one-line", "process handle SIGINT --pass true --stop false",
                 "--one-line", "process handle SIGUSR2 --pass true --stop false",
                 "--one-line", "thread continue"]
    else:
        setup = ["--one-line", "run"]
    return [lldb, "--batch"] + setup + on_crash + ["--"] + engine_argv


def lldb_exit_code(output):
    """The engine's real exit code parsed from lldb --batch output ("Process N
    exited with status = M"), or None if it did not exit cleanly -- i.e. it
    crashed, lldb ran the on-crash backtrace and quit 1, and there is no
    exit-status line (the backtrace is in `output`). Matches the Makefile/make.ps1
    lldb exit-code extraction."""
    match = re.search(r"Process \d+ exited with status = (\d+)", output)
    return int(match.group(1)) if match is not None else None


def run_under_lldb(binary, config, lldb, timeout, mem_limit_bytes):
    """Run the engine under lldb so a crash leaves a backtrace in the captured
    output. The normal mode is non-reproducible (real parallelism), so an
    in-the-moment stack is the only crash artifact -- re-running the seed need not
    crash again. The engine's real exit code comes from lldb's exit-status line
    (conservation failures exit 1, a clean run exits 0); no such line means the
    program crashed, which is a failure with the backtrace already captured."""
    timed_out, _lldb_rc, stdout, stderr = _capture(
        lldb_argv(lldb, run_command(binary, config)), timeout, mem_limit_bytes)
    if timed_out:
        return RunResult("timeout", None, None, stdout, stderr)
    combined = stdout + "\n" + stderr
    # Check for a crash FIRST: lldb's on-crash dump (frame variable / bt all) can
    # itself contain an "exited with status" string, so parsing the exit code
    # first could misread a crash as a clean exit. A "stop reason = signal" line
    # appears only on an actual stop -- the SIGUSR2/SIGINT pass-through never stops
    # -- so its presence is the reliable crash signal. The backtrace is already in
    # stdout/stderr -> the bundle.
    crash = re.search(r"stop reason = signal (\w+)", combined)
    if crash is not None:
        return RunResult("fail", None, crash.group(1), stdout, stderr)
    code = lldb_exit_code(combined)
    if code is None:
        # No crash signal and no clean-exit line: an aborted/failed run with
        # nothing to parse. Treat as a failure.
        return RunResult("fail", None, "crash", stdout, stderr)
    return RunResult("pass" if code == 0 else "fail", code, None, stdout, stderr)


def _decode(raw):
    return raw.decode(errors="replace") if raw else ""


def bundle_for(config, version, use_flags, argv, limits, result):
    """Assemble the failure-bundle dict: everything needed to reproduce and
    attribute a run from the record alone. `systematic_seed` is included only when
    the config carries one (systematic mode); the normal mode has none."""
    bundle = {
        "master_seed": config["master_seed"],
        "program_seed": config["workload"]["seed"],
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
    systematic_seed = config["runtime"].get("ponysystematictestingseed")
    if systematic_seed is not None:
        bundle["systematic_seed"] = systematic_seed
    return bundle


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
    # The cyclic and mesh workloads carry different shape fields (a cyclic config
    # has no `pingers`); chains/ttl/payload are shared. A systematic config has no
    # `workload` key (always mesh, by the engine default), so default to mesh.
    if shape.get("workload") == "cyclic":
        kind = "cyclic generations=%d group=%d" % (
            shape["generations"], shape["group"])
    else:
        kind = "mesh pingers=%d" % shape["pingers"]
    return "[seed %d] %s (%s chains=%d ttl=%d payload=%s) %s" % (
        config["master_seed"], result.outcome.upper(), kind,
        shape["chains"], shape["ttl"], shape["payload"], detail)


def execute(binary, config, version, use_flags, out_dir, timeout,
            mem_limit_bytes, runner=run_once):
    """Run one seed once; tee output, write a bundle on non-pass, return the
    RunResult. `runner` is the run strategy the driver injects, with signature
    `(binary, config, timeout, mem_limit_bytes) -> RunResult`: the systematic
    driver takes the default direct `run_once`; the normal driver passes an
    lldb-wrapped runner (see run_under_lldb). The captured stdout/stderr (which
    includes any crash backtrace) go to both the log and the bundle."""
    result = runner(binary, config, timeout, mem_limit_bytes)
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


def resolve_seeds(args):
    """The explicit seed list from --master-seed / --replay / --count / --seeds.
    (The normal driver's wall-clock --budget-seconds loop is separate -- it
    generates seeds incrementally rather than from a fixed list.)"""
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
