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

Cross-platform: this runs on Linux, macOS, AND Windows (the normal mode targets
all three TIER1 platforms; systematic is POSIX-only today only because its
Windows build is blocked, #5571). POSIX-only facilities (the RLIMIT_AS memory
cap) are gated here; the wall-clock watchdog is portable.

The pure pieces (derive_seed / resolve_workload / draw_* / build_argv /
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


def resolve_workload(rng, program_seed):
    """The workload shape: the program seed (passed, not drawn) plus the five
    swarm-drawn fields, in the dict-insertion and rng-draw order that is the
    seed-stability contract.

    `payload-mode` is deliberately NOT here -- each driver draws it via
    draw_payload_mode() AFTER its runtime-knob draws and BEFORE draw_max_threads().
    The interleaving of these draws in each driver's resolve_config is the
    contract: move any draw and every seed remaps (breaking historical --replay).
    resolve_config(0, 8) is pinned in the per-driver tests to guard it.

    These ranges are a starting point, not tuned. The choice() knobs sample every
    listed value equally; the randint() ranges (chains, ttl) under-sample their
    edges. When tuned, bias toward allocator size-class boundaries and reach past
    the current max of 64 pingers (contention bugs intensify with scale).
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
        runtime["ponygcinitial"] = rng.choice([1024, 1 << 16, 1 << 20])
    if rng.random() < 0.5:
        runtime["ponygcfactor"] = round(rng.choice([1.05, 1.5, 2.0, 4.0]), 2)
    if rng.random() < 0.5:
        runtime["ponycdinterval"] = rng.choice([10, 100, 1000])


def draw_payload_mode(rng):
    """`forward` re-sends one payload object down a chain (shared-val
    refcounting); `fresh` allocates a new one at each hop (allocation + collection
    churn). COUPLING: this must be drawn AFTER the runtime-knob draws and BEFORE
    draw_max_threads(), in BOTH drivers -- that position is the seed-stability
    contract. A driver that reorders it remaps every seed."""
    return rng.choice(["forward", "fresh"])


def draw_max_threads(rng, max_threads):
    """`--ponymaxthreads`, drawn LAST and the only host-dependent field, across
    [1, max_threads] so the swarm spans serial (1) through all physical cores. It
    MUST stay last: randint() consumes a variable number of rng steps (its width
    depends on max_threads), so any draw after it would remap across hosts with
    different core counts. The runtime REJECTS --ponymaxthreads > physical cores,
    so max_threads is the probed physical count (see probe_max_threads)."""
    return rng.randint(1, max_threads)


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


def _capture(argv, timeout, mem_limit_bytes):
    """Run argv under a wall-clock watchdog and (on POSIX) an RLIMIT_AS cap;
    return (timed_out, returncode, stdout, stderr). Mechanism only -- the caller
    classifies the outcome.

    The timeout is the primary guardrail on every platform -- a real-parallel or
    deterministic run can still deadlock. On timeout we kill the whole process
    GROUP, not just the direct child: the normal mode's direct child is lldb and
    the engine is its grandchild, so SIGKILLing only lldb would orphan a hung
    engine (it reparents to init and keeps consuming the host across the soak).
    `start_new_session` puts the child in its own group so killpg reaps the tree.

    The address-space cap (== `ulimit -v`, covering the pool allocator's mmap'd
    regions) is POSIX-only; on Windows the `resource` module is absent, so
    preexec_fn stays None and the job relies on the CI host's OOM handling."""
    preexec = None
    if (resource is not None) and (mem_limit_bytes is not None):
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
    return "[seed %d] %s (pingers=%d chains=%d ttl=%d payload=%s) %s" % (
        config["master_seed"], result.outcome.upper(), shape["pingers"],
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
