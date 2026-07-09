#!/usr/bin/env python3
"""Swarm orchestrator for the TCP stress engine (test/rt-stress/tcp-swarm).

Standalone: it does not import the generative harness. Each master seed draws one
TCP workload -- a random subset of the connection features plus magnitudes and a
thin runtime backdrop -- and runs the engine (`main.pony`) once. Omission is the
swarm mechanism: each feature is drawn independently, so different seeds enable
different subsets and push the net stack down different code paths (see
README.md). A failure (echo mismatch, crash, or hang) writes a bundle that
reproduces the run from its seed alone.

The run mechanism -- the no-progress watchdog (a run is a hang when its completed
count stops advancing) + process-group kill, a non-failing backstop that stops a
healthy over-long run, the RLIMIT_AS cap, and the optional lldb crash-backtrace
wrapper -- is adapted from the generative harness's `stress_common.py`; it is
copied here rather than shared so the two stress tests evolve independently (test
plumbing, low drift cost).
"""
import argparse
import json
import os
import random
import re
import shlex
import signal
import subprocess
import sys
import threading
import time

try:
    import resource  # POSIX only; absent on Windows
except ImportError:
    resource = None

BINARY_NAME = "tcp_swarm"
SOURCE_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(SOURCE_DIR)))

# The hang threshold: if the engine's HEARTBEAT `done=` count has not advanced for
# this long, the run is stuck (a stalled connection, a teardown that never finishes)
# and is killed as a failure. The engine emits a heartbeat on a fixed wall-clock
# timer, far more often than this, so a healthy-but-slow run always shows `done`
# advancing inside the window; only a genuine stall stops it.
DEFAULT_NO_PROGRESS_SECONDS = 300
# The backstop: a run still advancing at this point is stopped anyway so one seed
# can't run unbounded, but that is NOT a failure (outcome "incomplete") -- the run
# was healthy, it just ran long. Only the no-progress hang above fails a seed.
DEFAULT_TIMEOUT_SECONDS = 6000
# 14 GiB, on VIRTUAL address space (RLIMIT_AS caps virtual, not RSS). Pony's pool
# allocator reserves virtual in large MAP_NORESERVE arenas and holds the high-water mark
# under the in-flight connection backlog, so a run's virtual footprint runs far ahead of
# the RAM it touches -- RSS stayed ~120 MiB in every measurement below. The peak scales
# with how deep that backlog gets, which scales with how CPU-starved the run is: the
# failing 53k-connection seed measured ~5 GiB virtual on many fast cores but ~8.4 GiB
# pinned to 2 cores -- and the 2-core run reproduces the CI OOM exactly (same
# ponyint_virt_alloc abort). CI's slow 4-vCPU runner builds that deep backlog, and the old
# 8 GiB cap sat just below the ~8.4 GiB it needed. est_peak_bytes below estimates live
# WORKLOAD bytes (~1.2 GiB for this seed), ~4-7x under the real virtual peak, so the budget
# did not keep the run under the cap. Virtual is nearly free (RSS is the scarce resource and
# the runner has 16 GiB), so 14 clears the measured ~8.4 GiB worst case with margin at no RAM
# cost; a genuine runaway still trips it (virtual >= RSS) and the runner's own OOM-killer
# backstops. See the memory budget block below.
DEFAULT_MEM_LIMIT_MB = 14336


def info(message):
    print(message, flush=True)


def die(message):
    print("FATAL: " + message, file=sys.stderr, flush=True)
    sys.exit(1)


# ------------------------------------------------------------------- the draw

# Feature levers (each drawn independently -- omission is the swarm). Magnitudes
# are bucketed small/medium/large at 25/50/25 then uniform within the bucket.
WRITE_SHAPES = ["write", "writev"]
# writev buffer counts (writev only). 2048 exceeds POSIX IOV_MAX (1024): drawn
# with a payload at least that large, a single writev then queues more buffers
# than one writev() syscall can send, so TCPConnection takes its multi-batch send
# path -- the only path on which the mid-write yield (--yield-after-writing) fires
# on POSIX. 4 and 64 are ordinary small vector writes. The large value
# must stay above pony_os_writev_max()'s POSIX return, or the multi-batch coverage
# silently vanishes.
WRITEV_CHUNKS = [4, 64, 2048]
CLOSE_KINDS = ["graceful", "hard"]
# Payload sizes span the runtime's 16384-byte default read buffer: below, at, and
# above it, so runs straddle the read-chunk boundary rather than clustering small.
PAYLOAD_SIZES = [1, 8, 64, 256, 1024, 4096, 16384, 65536]
# Read buffer + yield thresholds: small values drive frequent yields back to the
# scheduler on any payload size; the 16384 default anchors the "unchanged" end.
READ_BUFFER_SIZES = [128, 1024, 16384, 65536]
YIELD_SIZES = [64, 1024, 16384]
CONCURRENCY = [8, 16, 32, 64, 128, 256]
# COUPLING: the max per-connection work -- max `messages` (64) below * max
# `payload-size` (65536) ~ 4 MiB, with concurrency >= 8 -- must keep the slowest single
# connection completing well within the orchestrator's no-progress window, or a healthy
# run reads as a hang.
MESSAGE_BUCKETS = {"small": (1, 2), "medium": (3, 16), "large": (17, 64)}
CONNECTION_BUCKETS = {"small": (100, 2000), "medium": (2001, 20000),
                      "large": (20001, 100000)}

# Per-run bounds -- BEST GUESSES, to be tuned from CI timings (the local WSL2
# loopback caps throughput at ~100 conns/s, which makes local run-time meaningless
# for this; memory below was measured locally). Two costs dominate: the number of
# round-trips (connections * messages) and the total bytes moved
# (connections * messages * payload) -- and each byte is generated by a per-position
# hash (the payload is a unique non-repeating stream, so it cannot be bulk-copied),
# so bytes are the heavier cost. The worst unclamped draw is 100k conns * 64 msgs *
# 64 KiB = ~400 GB, which would run for far too long; the clamp trims it. It keeps
# `connections` (the churn -- the point of the stress test) and `payload` (the drawn
# lever), trimming `messages` first, then `connections` as a last resort. Consumes
# no rng, so it does not perturb seed stability.
RUN_MAX_EXCHANGES = 2_000_000        # connections * messages round-trips
RUN_MAX_BYTES = 4_000_000_000        # ~4 GB moved (and hashed) per run
MIN_CONNECTIONS = 100


def clamp_run(connections, messages, payload):
    """Trim a drawn (connections, messages) to the per-run ceilings; see above."""
    per = max(1, payload)
    if (connections * messages) > RUN_MAX_EXCHANGES:
        messages = max(1, RUN_MAX_EXCHANGES // connections)
    if (connections * messages * per) > RUN_MAX_BYTES:
        messages = max(1, RUN_MAX_BYTES // (connections * per))
        if (connections * messages * per) > RUN_MAX_BYTES:
            connections = max(MIN_CONNECTIONS, RUN_MAX_BYTES // (messages * per))
    return connections, messages


def draw_bucketed(rng, buckets):
    """Draw a small/medium/large bucket at 25/50/25, then a uniform value in it.
    Exactly two rng draws, in that order (part of the seed-stability contract)."""
    roll = rng.random()
    if roll < 0.25:
        lo, hi = buckets["small"]
    elif roll < 0.75:
        lo, hi = buckets["medium"]
    else:
        lo, hi = buckets["large"]
    return rng.randint(lo, hi)


# --------------------------------------------------------------- memory budget

# The draw is trimmed to keep a workload's peak well under the RLIMIT_AS cap each seed
# runs under (see _capture / DEFAULT_MEM_LIMIT_MB). A workload over the cap is killed by
# the runtime's own out-of-memory abort (ponyint_virt_alloc) -- which reads like a runtime
# crash but is really the test asking for more address space than its cap allows, a false
# failure. Every memory-driving lever is drawn against a shared budget: the draw spends it,
# and once it is spent the remaining levers trim to fit. The budget bounds LIVE bytes,
# though, which runs well under the pool allocator's virtual high-water mark -- so it is a
# coverage-shaping trim, not a hard guarantee; the cap's slack (see DEFAULT_MEM_LIMIT_MB)
# is what actually absorbs the gap.
#
# The levers are drawn in a per-seed RANDOM order (see _draw_memory_levers), and that
# is the point. A fixed trim order would always shave the same lever -- we would never
# test a large writev-chunks, because it would always be the first cut -- throwing away
# swarm coverage. With a random order the trimmed lever rotates: on one seed
# writev-chunks wins the budget and concurrency/messages shrink, on another connections
# wins and writev-chunks is forced to 4. Every lever still reaches large on some
# fraction of seeds. This mirrors the generative harness's clamp_ttl (big chains force
# ttl small), generalized so the sacrificed lever is not always the same one.
#
# est_peak_bytes estimates peak live WORKLOAD bytes (pending write buffers, read buffers,
# bytes in flight, churn). This is NOT what the cap bounds: RLIMIT_AS caps virtual, and the
# pool allocator's virtual high-water mark under the in-flight backlog runs ~4-7x over these
# live-byte terms and grows as the run is CPU-starved -- a 53k-connection draw the budget put
# at ~1.2 GiB measured ~5 GiB virtual on fast cores and ~8.4 GiB pinned to 2 cores (~120 MiB
# RSS throughout), the 2-core run reproducing the CI OOM. The churn is what the budget misses;
# the term (connections * read_buffer) does not capture the backlog's pool reservation. We
# chose not to re-fit the constants to virtual -- pool reservation is a high-water artifact
# that varies with core count and stack, and would need re-measuring per stack (lori copies
# this model) -- and raised the cap to carry the slack instead. Re-fit only if you need the
# budget to be a tight bound.
# COUPLING: the constants track the engine's per-object and per-read-buffer memory --
# re-measure if make_chunks, the read buffer, or TCPConnection buffering changes.
MEM_BUDGET_BYTES = 2 * 1024 * 1024 * 1024   # 2 GiB workload, well under the 14 GiB cap
MEM_OBJ_BYTES = 2048        # per pending writev buffer object (margin over ~832 B virt)
MEM_RB_FACTOR = 4           # live read buffers: client + server, plus headroom

# The memory-driving levers, drawn in a shuffled order against MEM_BUDGET_BYTES.
# payload is here (concurrency * messages * payload reaches ~1 GiB); write-shape is
# NOT (it carries no magnitude, but it is drawn before these so chunks_eff is known).
MEM_LEVERS = ["connections", "concurrency", "messages", "payload",
              "writev_chunks", "read_buffer"]


def est_peak_bytes(concurrency, messages, payload, use_writev, writev_chunks,
                   read_buffer, connections):
    """Estimated peak workload memory (bytes) for a drawn config. Monotonic
    non-decreasing in each lever (flat in writev_chunks for a plain write), which is
    what makes the fit helpers below a simple clamp. See the block comment above."""
    chunks_eff = writev_chunks if use_writev else 1
    return (MEM_OBJ_BYTES * concurrency * messages * chunks_eff  # pending write bufs
            + MEM_RB_FACTOR * concurrency * read_buffer          # live read buffers
            + concurrency * messages * payload                   # bytes in flight
            + connections * read_buffer)                         # conservative churn


def _fit_discrete(key, drawn, chosen, values):
    """Largest value in `values` (ascending) that is <= `drawn` and keeps est_peak
    within budget, with the other levers at their `chosen` values. Precondition:
    `drawn` is a member of `values` (every caller draws it from the same list), so
    values[0] (the minimum) always fits and a fit always exists. Consumes no rng."""
    best = values[0]
    for v in values:
        if v > drawn:
            break
        trial = dict(chosen)
        trial[key] = v
        if est_peak_bytes(**trial) <= MEM_BUDGET_BYTES:
            best = v
    return best


def _fit_continuous(key, drawn, chosen, floor):
    """Largest integer in [floor, drawn] that keeps est_peak within budget, with the
    other levers at their `chosen` values. `floor` always fits, so a fit exists.
    Consumes no rng."""
    trial = dict(chosen)
    trial[key] = drawn
    if est_peak_bytes(**trial) <= MEM_BUDGET_BYTES:
        return drawn
    lo, hi, best = floor, drawn, floor
    while lo <= hi:
        mid = (lo + hi) // 2
        trial[key] = mid
        if est_peak_bytes(**trial) <= MEM_BUDGET_BYTES:
            best, lo = mid, mid + 1
        else:
            hi = mid - 1
    return best


def _draw_memory_levers(rng, use_writev):
    """Draw the memory levers in a per-seed random order against MEM_BUDGET_BYTES.
    Undrawn levers reserve their minimum, so every pick leaves room for the rest and
    the draw can never wedge (all-minimums is ~80 KB, far under budget). Each lever
    consumes its usual draw once; the fit is a no-rng clamp, so total rng consumption
    is fixed per seed. Returns the chosen values under internal keys."""
    chosen = {
        "connections": MIN_CONNECTIONS,
        "concurrency": min(CONCURRENCY),
        "messages": MESSAGE_BUCKETS["small"][0],
        "payload": min(PAYLOAD_SIZES),
        "writev_chunks": min(WRITEV_CHUNKS),
        "read_buffer": min(READ_BUFFER_SIZES),
        "use_writev": use_writev,
    }
    order = list(MEM_LEVERS)
    rng.shuffle(order)
    for lever in order:
        if lever == "connections":
            chosen[lever] = _fit_continuous(
                lever, draw_bucketed(rng, CONNECTION_BUCKETS), chosen,
                MIN_CONNECTIONS)
        elif lever == "messages":
            chosen[lever] = _fit_continuous(
                lever, draw_bucketed(rng, MESSAGE_BUCKETS), chosen,
                MESSAGE_BUCKETS["small"][0])
        elif lever == "concurrency":
            chosen[lever] = _fit_discrete(
                lever, rng.choice(CONCURRENCY), chosen, CONCURRENCY)
        elif lever == "payload":
            chosen[lever] = _fit_discrete(
                lever, rng.choice(PAYLOAD_SIZES), chosen, PAYLOAD_SIZES)
        elif lever == "writev_chunks":
            chosen[lever] = _fit_discrete(
                lever, rng.choice(WRITEV_CHUNKS), chosen, WRITEV_CHUNKS)
        elif lever == "read_buffer":
            chosen[lever] = _fit_discrete(
                lever, rng.choice(READ_BUFFER_SIZES), chosen, READ_BUFFER_SIZES)
    return chosen


def resolve_config(master_seed, max_threads, max_connections=None):
    """Draw one TCP workload from a master seed. The draw is the seed-stability
    contract: change it and every seed remaps (breaking a historical --replay), so
    change it deliberately and regenerate the pinned goldens in orchestrate_tcp_test.py
    (resolve_config(0, 8) etc.). It stays deterministic per seed, so --replay holds.
    The memory levers are drawn in a per-seed SHUFFLED order against a memory budget
    (see _draw_memory_levers) -- that shuffle is seeded only from master_seed, so it is
    host-independent. --ponymaxthreads is drawn LAST because it is the only
    host-dependent field (its randint width depends on the core count), so any draw
    after it would remap across hosts.

    max_connections, if set, caps the drawn connection count after the draw (no rng
    consumed, so within-host seed stability holds). Windows CI passes a small value
    because Windows opens TCP connections very slowly -- ~5.5/s, so 10k connections
    is ~30 min and 100k would never finish inside the job's time budget."""
    rng = random.Random(master_seed)

    workload = {}
    # write-shape is drawn BEFORE the memory levers: it carries no magnitude, but the
    # memory budget needs to know whether a writev-chunks draw counts (chunks_eff is 1
    # for a plain write). The memory levers then draw in a shuffled order against the
    # budget so no config can exceed the RLIMIT_AS cap; see _draw_memory_levers.
    use_writev = rng.choice(WRITE_SHAPES) == "writev"
    mem = _draw_memory_levers(rng, use_writev)
    payload = mem["payload"]
    read_buffer = mem["read_buffer"]
    workload["connections"] = mem["connections"]
    workload["concurrency"] = mem["concurrency"]
    workload["payload-size"] = payload
    workload["messages"] = mem["messages"]
    workload["write-shape"] = "writev" if use_writev else "write"
    workload["writev-chunks"] = mem["writev_chunks"]
    workload["read-buffer-size"] = read_buffer
    workload["yield-after-reading"] = rng.choice(YIELD_SIZES)
    workload["yield-after-writing"] = rng.choice(YIELD_SIZES)
    # expect: on/off. When on, the frame must not exceed the read buffer (the
    # engine's expect() errors above it), so clamp to it -- the oracle is
    # positional over the whole stream, so any valid frame size verifies.
    if rng.random() < 0.5:
        workload["expect"] = min(payload, read_buffer)
    else:
        workload["expect"] = 0
    workload["close"] = rng.choice(CLOSE_KINDS)

    # Post-draw clamp to the per-run ceilings (consumes no rng, so seeds stay
    # stable). Keeps connections and payload; trims messages, then connections.
    workload["connections"], workload["messages"] = clamp_run(
        workload["connections"], workload["messages"], payload)
    # Hard connection cap (Windows, where opens are slow). Applied after the ceiling
    # clamp; only lowers connections, so both ceilings still hold. No rng consumed.
    if max_connections is not None:
        workload["connections"] = min(workload["connections"], max_connections)

    runtime = {}
    if rng.random() < 0.5:
        runtime["ponynoscale"] = True
    # Pin the ASIO thread on a minority of runs -- only meaningful with live
    # sockets, so nothing else in the stress suite draws it.
    if rng.random() < 0.3:
        runtime["ponypin"] = True
        if rng.random() < 0.5:
            runtime["ponypinasio"] = True
    # Cycle detector off on ~half of runs. Not a GC test -- it is a leak check for
    # the connection lifecycle: with the detector on, a reference cycle in
    # TCPConnection teardown would be collected silently; with it off, that cycle
    # would leak the connection actor under churn (caught by the RLIMIT_AS cap or a
    # stall). Clean connection code is ORCA-collectable and passes either way. TCP
    # churn creates/destroys actors fast, so this is a TCP-specific reason to vary
    # it -- unlike the generative harness, which never opens a socket.
    if rng.random() < 0.5:
        runtime["ponynoblock"] = True
    runtime["ponymaxthreads"] = rng.randint(1, max_threads)  # LAST (see above)

    return {"master_seed": master_seed, "workload": workload, "runtime": runtime}


def build_argv(binary, config):
    """Full command line: workload args first, then runtime flags. A True value is
    a bare flag (`--ponynoscale`); anything else is `--key value`."""
    argv = [binary]
    for key, value in config["workload"].items():
        argv += ["--" + key, str(value)]
    for key, value in config["runtime"].items():
        if value is True:
            argv.append("--" + key)
        else:
            argv += ["--" + key, str(value)]
    return argv


def parse_result(stdout):
    """Extract the engine's RESULT tally, or {}."""
    result = {}
    for key in ("connections", "spawned", "completed", "failed", "verified",
                "mismatched"):
        # \b so `failed=` doesn't match inside the FAIL line's `connect_failed=`
        # (both `failed` and `mismatched` also appear there). The RESULT line is
        # printed first, so the leading-boundary anchor keeps us on it.
        match = re.search(r"\b" + key + r"=(\d+)", stdout)
        if match is not None:
            result[key] = int(match.group(1))
    return result


# ------------------------------------------------------------- run mechanism

def _rlimit_as_supported(platform, resource_available):
    """Install the RLIMIT_AS cap only on Linux (the one TIER1 platform validated
    to honor it; macOS raises in preexec, Windows lacks `resource`)."""
    return resource_available and (platform == "linux")


def _kill_process_tree(proc):
    if os.name == "posix":
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            return
        except (ProcessLookupError, PermissionError):
            pass
    proc.kill()


def _watchdog_kill_reason(now, start, last_progress, timeout,
                          no_progress_seconds):
    """Why the run should be killed, or None to keep waiting.

    "no_progress" -- the completed count has not advanced for the no-progress
    window: a hang (a stalled connection, a teardown that never finishes). This is
    the only unhealthy signal, and it fails the seed.

    "backstop" -- the run is still advancing but has passed the absolute cap: a
    healthy run that ran long. It is stopped so one seed can't take down the whole
    job, but it is not a failure. Checked second, so a real hang that happens to be
    past the cap is still reported as a hang."""
    if (now - last_progress) > no_progress_seconds:
        return "no_progress"
    if (now - start) > timeout:
        return "backstop"
    return None


# HEARTBEAT lines carry the engine's completed-connection count; a run makes
# progress when this advances (see _watch_for_progress). Bytes pattern: the reader
# threads read the child's pipes in binary.
_DONE_RE = re.compile(rb"HEARTBEAT done=(\d+)")


def _parse_done(line):
    """The completed-connection count from an engine HEARTBEAT line (bytes), or
    None if the line is not a heartbeat."""
    match = _DONE_RE.search(line)
    return int(match.group(1)) if match is not None else None


def _is_progress(done, max_done):
    """A heartbeat's completed count is progress only when it exceeds the max seen so
    far. This is the whole point of the done-count watchdog: a run that keeps printing
    heartbeats with a frozen (or absent) count is NOT making progress and is caught as
    a hang -- an output-based watchdog would be reset by the same lines and miss it."""
    return (done is not None) and (done > max_done)


def _watch_for_progress(proc, timeout, no_progress_seconds,
                        poll=time.monotonic, sleep=time.sleep):
    """Drain both streams in reader threads (so a full pipe can't deadlock the
    child). Progress is the engine's HEARTBEAT `done=` count advancing, not raw
    output -- a run whose count stalls for the no-progress window is a hang, even
    though its timer heartbeat keeps printing. Kill on a hang or on the absolute
    backstop. Returns (kill_reason, returncode, stdout, stderr): kill_reason is None
    on a clean exit, else "no_progress" (a hang) or "backstop" (a healthy over-long
    run)."""
    start = poll()
    last_progress = [start]
    max_done = [-1]
    lock = threading.Lock()
    chunks = {"out": [], "err": []}

    def drain(stream, key, track_progress):
        try:
            for line in iter(stream.readline, b""):
                chunks[key].append(line)
                if track_progress:
                    done = _parse_done(line)
                    with lock:
                        if _is_progress(done, max_done[0]):
                            max_done[0] = done
                            last_progress[0] = poll()
        finally:
            stream.close()

    readers = [
        threading.Thread(target=drain, args=(proc.stdout, "out", True),
                         daemon=True),
        threading.Thread(target=drain, args=(proc.stderr, "err", False),
                         daemon=True),
    ]
    for t in readers:
        t.start()

    kill_reason = None
    while proc.poll() is None:
        now = poll()
        with lock:
            last = last_progress[0]
        kill_reason = _watchdog_kill_reason(now, start, last, timeout,
                                            no_progress_seconds)
        if kill_reason is not None:
            _kill_process_tree(proc)
            break
        sleep(0.5)
    proc.wait()
    for t in readers:
        t.join(timeout=5)
    stdout = _decode(b"".join(chunks["out"]))
    stderr = _decode(b"".join(chunks["err"]))
    returncode = None if kill_reason is not None else proc.returncode
    return (kill_reason, returncode, stdout, stderr)


def _capture(argv, timeout, mem_limit_bytes, no_progress_seconds):
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
    return _watch_for_progress(proc, timeout, no_progress_seconds)


def _decode(raw):
    return raw.decode(errors="replace") if raw else ""


def lldb_argv(lldb, engine_argv):
    """Wrap the engine argv under `lldb --batch` so a crash leaves a backtrace.
    POSIX passes SIGINT/SIGUSR2 through without stopping (the runtime uses them for
    shutdown/scheduler wakeups); everything else stops the process, which is the
    crash we want to capture."""
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
    match = re.search(r"Process \d+ exited with status = (\d+)", output)
    return int(match.group(1)) if match is not None else None


class RunResult:
    def __init__(self, outcome, returncode, signal_, stdout, stderr):
        self.outcome = outcome  # "pass" | "fail" | "hang" | "incomplete"
        self.returncode = returncode
        self.signal = signal_
        self.stdout = stdout
        self.stderr = stderr


def _classify_outcome(kill_reason, returncode):
    """A run's outcome from its kill reason (or clean exit). Among the watchdog
    kills only a hang is a failure; the backstop stops a healthy over-long run
    without failing it. A clean exit is pass/fail by the exit code."""
    if kill_reason == "no_progress":
        return "hang"
    if kill_reason == "backstop":
        return "incomplete"
    return "pass" if returncode == 0 else "fail"


def _is_failure(outcome):
    """A seed fails only if it crashed or mismatched ("fail") or hung ("hang").
    "pass" and "incomplete" (a healthy run stopped at the backstop) do not."""
    return outcome in ("fail", "hang")


def run_once(binary, config, timeout, mem_limit_bytes, no_progress_seconds):
    """Direct run: classify from the kill reason, else the exit code."""
    kill_reason, returncode, stdout, stderr = _capture(
        build_argv(binary, config), timeout, mem_limit_bytes,
        no_progress_seconds)
    if kill_reason is not None:
        return RunResult(_classify_outcome(kill_reason, None), None, None,
                         stdout, stderr)
    sig = -returncode if returncode < 0 else None
    return RunResult(_classify_outcome(None, returncode), returncode, sig,
                     stdout, stderr)


def run_under_lldb(binary, lldb, config, timeout, mem_limit_bytes,
                   no_progress_seconds):
    """Run under lldb so a crash leaves an in-the-moment backtrace."""
    kill_reason, _rc, stdout, stderr = _capture(
        lldb_argv(lldb, build_argv(binary, config)), timeout, mem_limit_bytes,
        no_progress_seconds)
    if kill_reason is not None:
        return RunResult(_classify_outcome(kill_reason, None), None, None,
                         stdout, stderr)
    combined = stdout + "\n" + stderr
    crash = re.search(r"stop reason = signal (\w+)", combined)
    if crash is not None:
        return RunResult("fail", None, crash.group(1), stdout, stderr)
    code = lldb_exit_code(combined)
    if code is None:
        return RunResult("fail", None, "crash", stdout, stderr)
    return RunResult("pass" if code == 0 else "fail", code, None, stdout, stderr)


def probe_max_threads(binary):
    """The runtime's physical core count -- the ceiling --ponymaxthreads is checked
    against. Ask for an impossible count and parse the number the runtime rejects
    with, matching its own (SMT-collapsing) definition. Falls back to cpu_count."""
    cmd = [binary, "--ponymaxthreads", "1000000", "--ponynoscale",
           "--connections", "0", "--concurrency", "1"]
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
    info("note: could not probe physical cores; using os.cpu_count()=%d"
         % fallback)
    return fallback


def ponyc_version(ponyc):
    result = subprocess.run([ponyc, "--version"], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    return result.stdout.decode(errors="replace").strip()


def compile_engine(ponyc, out_dir):
    """Compile the engine once (always `-d`). stdlib resolves from
    REPO_ROOT/packages."""
    env = dict(os.environ, PONYPATH=os.path.join(REPO_ROOT, "packages"))
    info("+ compiling engine with " + ponyc)
    result = subprocess.run([ponyc, "-d", "-b", BINARY_NAME, "--pic", "-o",
                             out_dir, SOURCE_DIR], env=env)
    if result.returncode != 0:
        die("compiling the engine failed")
    binary = os.path.join(out_dir, BINARY_NAME)
    if os.name == "nt":
        binary += ".exe"
    if not os.path.isfile(binary):
        die("engine binary not produced at " + binary)
    return binary


def summary_line(config, result):
    parsed = parse_result(result.stdout)
    shape = config["workload"]
    detail = ("connections=%s completed=%s failed=%s verified=%s mismatched=%s"
              % (parsed.get("connections", "?"), parsed.get("completed", "?"),
                 parsed.get("failed", "?"), parsed.get("verified", "?"),
                 parsed.get("mismatched", "?")))
    return ("[seed %d] %s (payload=%s messages=%s %s expect=%s close=%s) %s"
            % (config["master_seed"], result.outcome.upper(),
               shape["payload-size"], shape["messages"], shape["write-shape"],
               shape["expect"], shape["close"], detail))


def bundle_for(config, version, argv, limits, result):
    return {
        "master_seed": config["master_seed"],
        "workload": config["workload"],
        "runtime_flags": config["runtime"],
        "cli": " ".join(shlex.quote(a) for a in argv),
        "ponyc_version": version,
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


def execute(binary, config, version, out_dir, timeout, mem_limit_bytes,
            no_progress_seconds, lldb):
    if lldb is not None:
        result = run_under_lldb(binary, lldb, config, timeout, mem_limit_bytes,
                                no_progress_seconds)
    else:
        result = run_once(binary, config, timeout, mem_limit_bytes,
                          no_progress_seconds)
    info(summary_line(config, result))
    if result.stdout:
        info(result.stdout.rstrip("\n"))
    if result.stderr:
        info(result.stderr.rstrip("\n"))
    if _is_failure(result.outcome):
        argv = (lldb_argv(lldb, build_argv(binary, config)) if lldb is not None
                else build_argv(binary, config))
        limits = {"timeout_seconds": timeout, "mem_limit_bytes": mem_limit_bytes}
        path = write_bundle(out_dir,
                            bundle_for(config, version, argv, limits, result))
        info("wrote failure bundle: " + path)
    return result


def resolve_seeds(args):
    if args.master_seed is not None:
        return [args.master_seed]
    if args.replay is not None:
        return [args.replay]
    if args.count is not None:
        return list(range(args.start, args.start + args.count))
    if args.seeds is not None:
        try:
            return [int(token) for token in args.seeds.split(",")]
        except ValueError:
            die("bad --seeds (expected comma-separated integers): " + args.seeds)
    # No selector given: run the single --start seed (default 0).
    return [args.start]


def main():
    parser = argparse.ArgumentParser(description="Swarm TCP stress orchestrator")
    parser.add_argument("--ponyc", required=True, help="path to a debug ponyc")
    parser.add_argument("--out", default=os.path.join(os.path.expanduser("~"),
                        "tmp", "tcp-swarm-out"), help="output dir for bundles")
    parser.add_argument("--lldb", default=None,
                        help="run each seed under this lldb (crash backtraces)")
    parser.add_argument("--start", type=int, default=0,
                        help="first seed for --count / --budget-seconds")
    # The seed selectors are mutually exclusive -- combining them silently ignored
    # all but one. With none given, resolve_seeds runs the single --start seed.
    selector = parser.add_mutually_exclusive_group()
    selector.add_argument("--count", type=int, default=None,
                          help="run --count seeds from --start")
    selector.add_argument("--master-seed", type=int, default=None,
                          help="run this single seed")
    selector.add_argument("--seeds", default=None,
                          help="run this comma-separated list of seeds")
    selector.add_argument("--replay", type=int, default=None,
                          help="reproduce this seed's WORKLOAD exactly; note "
                               "--ponymaxthreads is re-drawn against the local "
                               "core count, so the runtime backdrop can differ "
                               "across hosts")
    selector.add_argument("--budget-seconds", type=int, default=None,
                          help="run seeds from --start until this many seconds "
                               "pass")
    parser.add_argument("--no-progress-seconds", type=int,
                        default=DEFAULT_NO_PROGRESS_SECONDS,
                        help="hang threshold: fail a run whose completed count has "
                             "not advanced for this long")
    parser.add_argument("--timeout-seconds", type=int,
                        default=DEFAULT_TIMEOUT_SECONDS,
                        help="non-failing backstop: a run still advancing at this "
                             "point is stopped (outcome 'incomplete'), not failed")
    parser.add_argument("--mem-limit-mb", type=int, default=DEFAULT_MEM_LIMIT_MB)
    parser.add_argument("--max-connections", type=int, default=None,
                        help="cap each seed's drawn connection count (Windows CI "
                             "passes a small value -- Windows opens sockets slowly)")
    args = parser.parse_args()
    if args.max_connections is not None and args.max_connections <= 0:
        die("--max-connections must be positive (got %d)" % args.max_connections)

    os.makedirs(args.out, exist_ok=True)
    binary = compile_engine(args.ponyc, args.out)
    version = ponyc_version(args.ponyc)
    max_threads = probe_max_threads(binary)
    info("probed max threads: %d" % max_threads)
    mem_limit_bytes = (args.mem_limit_mb * 1024 * 1024
                       if args.mem_limit_mb > 0 else None)

    failures = []

    def run_seed(seed):
        config = resolve_config(seed, max_threads, args.max_connections)
        result = execute(binary, config, version, args.out,
                         args.timeout_seconds, mem_limit_bytes,
                         args.no_progress_seconds, args.lldb)
        if _is_failure(result.outcome):
            failures.append(seed)

    if args.budget_seconds is not None:
        if args.budget_seconds <= 0:
            die("--budget-seconds must be positive (got %d)" % args.budget_seconds)
        start_time = time.monotonic()
        seed = args.start
        while (time.monotonic() - start_time) < args.budget_seconds:
            run_seed(seed)
            seed += 1
    else:
        for seed in resolve_seeds(args):
            run_seed(seed)

    if failures:
        die("failures on seeds: " + ", ".join(str(s) for s in failures))
    info("all seeds passed")


if __name__ == "__main__":
    main()
