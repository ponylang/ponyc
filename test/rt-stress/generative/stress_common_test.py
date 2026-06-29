#!/usr/bin/env python3
"""Unit tests for the pure, mode-agnostic pieces of stress_common.py.

Self-contained (no pytest): runnable as
`python3 test/rt-stress/generative/stress_common_test.py`, exits 0 on pass / 1 on
failure. Picked up by lint-python.yml's `*_test.py` discovery.
"""
import os
import random
import sys
import types

import stress_common as common

FAILURES = []

U64_MAX = (1 << 64) - 1

# A minimal valid config for exercising the run classifiers; _capture is faked,
# so the values only need to let run_command/build_argv run without error.
_MIN_CONFIG = {
    "master_seed": 0,
    "workload": {"seed": 9, "pingers": 1, "chains": 1, "ttl": 0,
                 "payload": "u64", "payload-size": 1, "payload-mode": "fresh"},
    "runtime": {"ponymaxthreads": 1},
}


def check(name, condition):
    if condition:
        print("ok   - " + name)
    else:
        print("FAIL - " + name)
        FAILURES.append(name)


def test_derive_seed():
    check("derive_seed is deterministic",
          common.derive_seed(42, "program") == common.derive_seed(42, "program"))

    # Stays in [1, U64_MAX] across a spread of masters. The >= 1 floor is
    # load-bearing -- the runtime rejects systematic seed 0 and Rand(0,0) is the
    # degenerate all-zero xoroshiro state.
    all_in_range = True
    seen = set()
    for master in range(0, 300):
        program = common.derive_seed(master, "program")
        seen.add(program)
        if not (1 <= program <= U64_MAX):
            all_in_range = False
    check("derive_seed keeps the seed in [1, U64_MAX]", all_in_range)
    check("derive_seed varies with the master seed", len(seen) > 250)

    # Different labels yield independent seeds from the same master.
    check("derive_seed: different labels differ",
          common.derive_seed(0, "program") != common.derive_seed(0, "systematic"))


def test_derive_seed_zero_floor():
    # No real SHA-256 prefix is zero in the seed ranges used, so force a zero
    # digest to exercise the `or 1` floor directly (it is otherwise never hit).
    class _ZeroDigest:
        def digest(self):
            return b"\x00" * 32

    real = common.hashlib.sha256
    try:
        common.hashlib.sha256 = lambda *a, **k: _ZeroDigest()
        check("derive_seed floors a zero digest to 1",
              common.derive_seed(123, "program") == 1)
    finally:
        common.hashlib.sha256 = real


def test_resolve_systematic_workload_five_field_split():
    # CRITICAL contract: resolve_systematic_workload returns the program seed plus
    # EXACTLY the five swarm-drawn fields -- payload-mode is NOT here (the systematic
    # driver draws it after its runtime knobs). A regression that folds payload-mode
    # back in (a contiguous-six draw) remaps every seed and breaks systematic replay;
    # this pins against it.
    rng = random.Random(0)
    work = common.resolve_systematic_workload(rng, 999)
    check("resolve_systematic_workload keys are seed + the five drawn fields",
          set(work.keys()) == {"seed", "pingers", "chains", "ttl", "payload",
                               "payload-size"})
    check("resolve_systematic_workload does NOT include payload-mode",
          "payload-mode" not in work)
    check("resolve_systematic_workload carries the passed program seed",
          work["seed"] == 999)
    # Pin the exact draw order/values for Random(0): a reordered or narrowed draw
    # changes this (and would silently remap historical seeds).
    check("resolve_systematic_workload(Random(0), 999) matches the pinned draws",
          work == {"seed": 999, "pingers": 64, "chains": 198, "ttl": 48,
                   "payload": "u64", "payload-size": 1})


def test_resolve_systematic_workload_coverage():
    pingers_allowed = {1, 2, 4, 8, 16, 32, 64}
    size_allowed = {1, 8, 64, 256, 1024, 4096}
    pingers_seen, size_seen, payload_seen = set(), set(), set()
    chains_seen, ttls_seen = [], []
    invariants = True
    for master in range(0, 300):
        rng = random.Random(master)
        w = common.resolve_systematic_workload(rng, 1)
        if w["pingers"] not in pingers_allowed:
            invariants = False
        if not (1 <= w["chains"] <= 400):
            invariants = False
        if not (0 <= w["ttl"] <= 48):
            invariants = False
        if w["payload"] not in ("string", "u64"):
            invariants = False
        if w["payload-size"] not in size_allowed:
            invariants = False
        pingers_seen.add(w["pingers"])
        size_seen.add(w["payload-size"])
        payload_seen.add(w["payload"])
        chains_seen.append(w["chains"])
        ttls_seen.append(w["ttl"])
    check("resolve_systematic_workload holds all field invariants over 300 seeds",
          invariants)
    check("pingers covers the full declared set", pingers_seen == pingers_allowed)
    check("payload covers {string, u64}", payload_seen == {"string", "u64"})
    check("payload-size covers the full declared set", size_seen == size_allowed)
    check("ttl reaches both declared edges 0 and 48",
          (min(ttls_seen) == 0) and (max(ttls_seen) == 48))
    check("chains spans most of [1,400]",
          (max(chains_seen) - min(chains_seen)) >= 380)


def test_draw_swarm_knobs():
    # Omission IS the mechanism: each of the four optional knobs must appear in
    # some runs and be absent in others. They share identical `if rng.random() <
    # 0.5` logic, so test all four (a regression in any one would otherwise pass).
    # ponynoblock is NOT one of them -- draw_swarm_knobs must never add it.
    omittable = ["ponynoscale", "ponygcinitial", "ponygcfactor", "ponycdinterval"]
    counts = dict.fromkeys(omittable, 0)
    ponynoblock_added = False
    gcinitial_values = set()
    for master in range(0, 300):
        runtime = {}
        common.draw_swarm_knobs(random.Random(master), runtime)
        if "ponynoblock" in runtime:
            ponynoblock_added = True
        if "ponygcinitial" in runtime:
            gcinitial_values.add(runtime["ponygcinitial"])
        for knob in omittable:
            if knob in runtime:
                counts[knob] += 1
    for knob in omittable:
        check("swarm omission: %s sometimes present" % knob, counts[knob] > 0)
        check("swarm omission: %s sometimes absent" % knob, counts[knob] < 300)
    check("draw_swarm_knobs never adds ponynoblock", not ponynoblock_added)
    # 0 is the deliberate constant-GC stress point (a 1-byte threshold). Unlike
    # the other swarm knobs we pin its presence in the draw -- without this a
    # refactor could silently drop the regime the value exists to exercise.
    check("swarm draws the gcinitial=0 constant-GC stress point",
          0 in gcinitial_values)


def test_draw_payload_mode():
    seen = set()
    for master in range(0, 100):
        seen.add(common.draw_payload_mode(random.Random(master)))
    check("draw_payload_mode covers {forward, fresh}",
          seen == {"forward", "fresh"})


def test_draw_max_threads():
    seen = set()
    ok = True
    for master in range(0, 300):
        mt = common.draw_max_threads(random.Random(master), 8)
        if not (1 <= mt <= 8):
            ok = False
        seen.add(mt)
    check("draw_max_threads always in [1, max]", ok)
    check("draw_max_threads spans the full [1, max] range",
          seen == set(range(1, 9)))


def test_draw_bucketed():
    # Each draw picks a small/medium/large bucket at 25/50/25, then a uniform value
    # within it. Values must always land in exactly one declared (non-overlapping)
    # bucket, all three must be reachable, and the weighting must be ~25/50/25.
    b = common.NORMAL_SIZE_BUCKETS
    counts = {"small": 0, "medium": 0, "large": 0}
    in_range = True
    for master in range(0, 2000):
        v = common.draw_bucketed(random.Random(master), b)
        if b["small"][0] <= v <= b["small"][1]:
            counts["small"] += 1
        elif b["medium"][0] <= v <= b["medium"][1]:
            counts["medium"] += 1
        elif b["large"][0] <= v <= b["large"][1]:
            counts["large"] += 1
        else:
            in_range = False
    check("draw_bucketed values always land in one declared bucket", in_range)
    check("draw_bucketed reaches all three buckets",
          all(c > 0 for c in counts.values()))
    # 2000 draws at 25/50/25: ~500/1000/500. Deterministic, loose bounds.
    check("draw_bucketed weighting is ~25/50/25",
          (400 <= counts["small"] <= 650)
          and (850 <= counts["medium"] <= 1150)
          and (400 <= counts["large"] <= 650))


def test_draw_payload():
    # Size is drawn freely from the full set now -- run time is bounded by clamp_ttl
    # (which trims ttl), not by restricting which sizes a big run may pick. Every
    # size, kind, and mode must be reachable across seeds.
    sizes, kinds, modes = set(), set(), set()
    for master in range(0, 2000):
        k, s, m = common.draw_payload(random.Random(master))
        sizes.add(s)
        kinds.add(k)
        modes.add(m)
    check("draw_payload covers every string size", sizes == set(common.STRING_SIZES))
    check("draw_payload covers {string, u64}", kinds == {"string", "u64"})
    check("draw_payload covers {forward, fresh}", modes == {"forward", "fresh"})

    # Seed-stability contract: exactly three rng draws -- kind, mode, size -- in that
    # order, so draw_payload never remaps a downstream draw. Compare the rng state
    # after the call to a hand-rolled three-draw reference.
    stable = True
    for master in range(0, 300):
        rng = random.Random(master)
        common.draw_payload(rng)
        ref = random.Random(master)
        ref.choice(["string", "u64"])
        ref.choice(["forward", "fresh"])
        ref.random()
        if rng.getstate() != ref.getstate():
            stable = False
    check("draw_payload consumes exactly three rng draws (kind, mode, size)", stable)


def test_clamp_ttl():
    # The mesh run-time bound. Forward cost is quadratic in chains, fresh linear, so
    # est must separate them; clamp_ttl trims ttl so the estimate fits the ceiling.
    C = common.RUN_TIME_CEILING_SECONDS
    check("est: forward cost is >>(10x) fresh at high chains",
          common.est_mesh_seconds(30000, 1000, "forward", "u64", 1)
          > 10 * common.est_mesh_seconds(30000, 1000, "fresh", "u64", 1))

    # An in-budget config is returned unchanged.
    check("clamp_ttl leaves an in-budget config alone",
          common.clamp_ttl(2000, 1000, "fresh", "u64", 1) == 1000)

    # The original failing seed: forward u64, chains=31178, ttl=30244 (hours of run
    # time). It must clamp to a large-but-finite ttl whose estimate fits the ceiling.
    ct = common.clamp_ttl(31178, 30244, "forward", "u64", 256)
    check("clamp_ttl bounds the failing forward seed", ct < 30244)
    check("clamp_ttl leaves a usable ttl for the failing seed", ct > 0)
    check("clamp_ttl keeps the failing seed's estimate within the ceiling",
          common.est_mesh_seconds(31178, ct, "forward", "u64", 256) <= C)

    # Never raises ttl; floors at 0 (never negative) even at the most extreme config.
    over = common.clamp_ttl(34000, 34000, "forward", "string", 4096)
    check("clamp_ttl never raises ttl", over <= 34000)
    check("clamp_ttl floors at 0 (never negative)", over >= 0)

    # Every (mode, payload, size) at max chains AND max ttl clamps within the ceiling.
    worst_ok = True
    max_dim = common.NORMAL_SIZE_BUCKETS["large"][1]
    for mode in ("forward", "fresh"):
        for payload in ("u64", "string"):
            for size in common.STRING_SIZES:
                t = common.clamp_ttl(max_dim, max_dim, mode, payload, size)
                if common.est_mesh_seconds(max_dim, t, mode, payload, size) > C:
                    worst_ok = False
    check("clamp_ttl bounds every payload/size at max chains+ttl", worst_ok)


# The calibrated ceiling on generations*group (leaked actors in a CD-off run).
# Hardcoded, NOT derived from the CYCLIC_* constants, so bumping a generation
# bucket or adding a bigger group trips this guard and forces a re-measure. The
# real max today is 8000*16 = 128000 (~1.2 GB peak, fits under DEFAULT_MEM_LIMIT_MB
# with margin -- see the decision log / CYCLIC_* docstring).
CYCLIC_WORKER_CEILING = 130000


# The calibrated ceiling on producers*messages (total backpressure work).
# Hardcoded, NOT derived from the BACKPRESSURE_* constants, so bumping a producer
# count or a message bucket trips this guard and forces a time re-measure. Real max
# today is 256*400000 = 102_400_000.
BACKPRESSURE_MESSAGE_CEILING = 110_000_000


class _CallCountingRandom(random.Random):
    """A Random that counts the high-level draw calls (random/randint/choice) made
    on it, delegating to the real implementation so the produced values are
    unchanged. Used to verify draw_workload makes the same number of calls whichever
    kind it rolls."""

    def __init__(self, seed):
        super().__init__(seed)
        self.calls = 0

    def random(self):
        self.calls += 1
        return super().random()

    def randint(self, a, b):
        self.calls += 1
        return super().randint(a, b)

    def choice(self, seq):
        self.calls += 1
        return super().choice(seq)


def test_draw_workload():
    check("draw_workload is deterministic",
          common.draw_workload(random.Random(7))
          == common.draw_workload(random.Random(7)))

    kinds = set()
    bounds_ok = True
    gen_lo = common.CYCLIC_GENERATION_BUCKETS["small"][0]
    gen_hi = common.CYCLIC_GENERATION_BUCKETS["large"][1]
    bp_msg_lo = common.BACKPRESSURE_MESSAGE_BUCKETS["small"][0]
    bp_msg_hi = common.BACKPRESSURE_MESSAGE_BUCKETS["large"][1]
    for master in range(0, 500):
        (workload, generations, group, producers, messages, apply_every) = \
            common.draw_workload(random.Random(master))
        kinds.add(workload)
        if workload not in ("mesh", "cyclic", "backpressure"):
            bounds_ok = False
        # EVERY kind's params are drawn for every roll (fixed rng consumption), so
        # they are always valid numbers in range regardless of the rolled kind --
        # checking them over all seeds is the evidence the draw never skips any.
        if not (gen_lo <= generations <= gen_hi):
            bounds_ok = False
        if group not in common.CYCLIC_GROUPS:
            bounds_ok = False
        if producers not in common.BACKPRESSURE_PRODUCERS:
            bounds_ok = False
        if not (bp_msg_lo <= messages <= bp_msg_hi):
            bounds_ok = False
        if apply_every not in common.BACKPRESSURE_APPLY_EVERY:
            bounds_ok = False
    check("draw_workload covers all three workload kinds",
          kinds == {"mesh", "cyclic", "backpressure"})
    check("draw_workload all shape params always in range (drawn for every kind)",
          bounds_ok)
    # Ceiling guard: assert the THEORETICAL worst case (the product of the bound
    # maxima), NOT the sampled worst over the seeds. A narrow bucket bump can push
    # the theoretical max over a ceiling without any 500-seed sample reaching it
    # (the largest producers and the largest messages need not co-occur in a draw),
    # so a sampled check would miss it. The product of the constants always trips.
    cyclic_theoretical = (max(common.CYCLIC_GROUPS)
                          * common.CYCLIC_GENERATION_BUCKETS["large"][1])
    bp_theoretical = (max(common.BACKPRESSURE_PRODUCERS)
                      * common.BACKPRESSURE_MESSAGE_BUCKETS["large"][1])
    check("cyclic theoretical worst-case workers within the memory ceiling",
          cyclic_theoretical <= CYCLIC_WORKER_CEILING)
    check("backpressure theoretical worst-case messages within the ceiling",
          bp_theoretical <= BACKPRESSURE_MESSAGE_CEILING)

    # Fixed-consumption contract: draw_workload must make the SAME sequence of rng
    # calls whichever kind it rolls (it draws every kind's shape unconditionally),
    # so a future kind-dependent extra draw -- which would remap seeds -- is caught.
    # Compare the call COUNT across a seed of each kind; comparing rng state would
    # not work, because a randint's underlying bit consumption varies with its value
    # (see the draw_workload doc).
    counts = []
    for kind in ("mesh", "cyclic", "backpressure"):
        seed = next(s for s in range(0, 500)
                    if common.draw_workload(random.Random(s))[0] == kind)
        counter = _CallCountingRandom(seed)
        common.draw_workload(counter)
        counts.append(counter.calls)
    check("draw_workload makes the same number of rng calls for every kind",
          (len(set(counts)) == 1) and (counts[0] > 0))


def test_build_argv():
    config = {
        "master_seed": 0,
        "workload": {"seed": 5, "pingers": 8, "chains": 8, "ttl": 16,
                     "payload": "string", "payload-size": 64,
                     "payload-mode": "fresh"},
        "runtime": {"ponynoblock": True, "ponygcfactor": 1.05,
                    "ponymaxthreads": 2},
    }
    argv = common.build_argv("/bin/generative", config)
    check("build_argv: binary is first", argv[0] == "/bin/generative")
    check("build_argv: includes --ponynoblock bare", "--ponynoblock" in argv)
    # A bare flag must not leak its Python True as a literal token.
    check("build_argv: no literal True token", "True" not in argv)
    check("build_argv: --seed present", "--seed" in argv)
    if "--seed" in argv:
        check("build_argv: --seed carries the program seed",
              argv[argv.index("--seed") + 1] == "5")
    check("build_argv renders a float knob value",
          ("--ponygcfactor" in argv)
          and (argv[argv.index("--ponygcfactor") + 1] == "1.05"))
    check("build_argv renders an int knob value",
          ("--ponymaxthreads" in argv)
          and (argv[argv.index("--ponymaxthreads") + 1] == "2"))


def test_run_command():
    config = {
        "master_seed": 0,
        "workload": {"seed": 5, "pingers": 8, "chains": 8, "ttl": 16,
                     "payload": "string", "payload-size": 64,
                     "payload-mode": "fresh"},
        "runtime": {"ponynoblock": True, "ponymaxthreads": 2},
    }
    # No ASLR/setarch wrapper (systematic replay is layout-independent since
    # #5566/#5570; normal mode is non-reproducible), so run_command == the engine
    # argv.
    check("run_command equals the engine argv (no wrapper)",
          common.run_command("/bin/generative", config)
          == common.build_argv("/bin/generative", config))


def test_lldb_argv():
    argv = common.lldb_argv("/usr/bin/lldb", ["/bin/generative", "--seed", "5"])
    check("lldb_argv: lldb is first", argv[0] == "/usr/bin/lldb")
    check("lldb_argv: --batch present", "--batch" in argv)
    # The on-crash backtrace is what makes a non-reproducible crash diagnosable.
    check("lldb_argv: captures a backtrace on crash", "bt all" in argv)
    check("lldb_argv: quits non-zero on crash", "quit 1" in argv)
    # register read is deliberately dropped (raw addresses are useless without the
    # binary, which we don't upload).
    check("lldb_argv: register read dropped", "register read" not in argv)
    # The engine argv follows the `--` separator, intact.
    sep = argv.index("--")
    check("lldb_argv: engine argv follows --",
          argv[sep + 1:] == ["/bin/generative", "--seed", "5"])
    # POSIX MUST pass SIGUSR2 through without stopping -- the runtime uses it for
    # scheduler wakeups, so stopping on it hangs the run. (This test host is posix.)
    if os.name == "posix":
        joined = " ".join(argv)
        check("lldb_argv (posix): passes SIGUSR2 through without stopping",
              "process handle SIGUSR2 --pass true --stop false" in joined)
        check("lldb_argv (posix): stops at main to set up signals first",
              "breakpoint set --name main" in joined)


def test_lldb_exit_code():
    check("lldb_exit_code: clean exit 0",
          common.lldb_exit_code(
              "Process 12345 exited with status = 0 (0x00000000)") == 0)
    check("lldb_exit_code: conservation exit 1",
          common.lldb_exit_code(
              "Process 7 exited with status = 1 (0x00000001)") == 1)
    # A crash leaves no exit-status line -> None; the caller treats None as a
    # crash (the backtrace is elsewhere in the captured output).
    check("lldb_exit_code: crash (no exit line) -> None",
          common.lldb_exit_code(
              "Process 7 stopped\n* stop reason = signal SIGSEGV\nbt all...")
          is None)


def _fake_capture(timed_out, returncode, stdout, stderr=""):
    """A stand-in for common._capture that returns crafted output, so the run
    classifiers can be tested without launching a process."""
    return lambda *a, **k: (timed_out, returncode, stdout, stderr)


def test_run_once():
    real = common._capture
    try:
        common._capture = _fake_capture(False, 0, "ok")
        r = common.run_once("/bin/x", _MIN_CONFIG, 60, None)
        check("run_once: exit 0 -> pass",
              r.outcome == "pass" and r.returncode == 0 and r.signal is None)
        common._capture = _fake_capture(False, 1, "", "conservation")
        r = common.run_once("/bin/x", _MIN_CONFIG, 60, None)
        check("run_once: exit 1 -> fail", r.outcome == "fail" and r.returncode == 1)
        common._capture = _fake_capture(False, -11, "", "")
        r = common.run_once("/bin/x", _MIN_CONFIG, 60, None)
        check("run_once: negative code -> signal number 11",
              r.outcome == "fail" and r.signal == 11)
        common._capture = _fake_capture(True, None, "", "")
        r = common.run_once("/bin/x", _MIN_CONFIG, 60, None)
        check("run_once: timeout", r.outcome == "timeout")
    finally:
        common._capture = real


def test_run_under_lldb():
    # The load-bearing crash-classification: inject crafted lldb output and verify
    # each branch (the signal-name parse is the only crash artifact normal mode
    # gets).
    real = common._capture
    try:
        common._capture = _fake_capture(
            False, 0, "Process 7 exited with status = 0 (0x0)")
        r = common.run_under_lldb("/bin/x", _MIN_CONFIG, "lldb", 60, None)
        check("run_under_lldb: clean exit 0 -> pass",
              r.outcome == "pass" and r.returncode == 0)
        common._capture = _fake_capture(
            False, 0, "Process 7 exited with status = 1 (0x1)")
        r = common.run_under_lldb("/bin/x", _MIN_CONFIG, "lldb", 60, None)
        check("run_under_lldb: conservation exit 1 -> fail",
              r.outcome == "fail" and r.returncode == 1)
        common._capture = _fake_capture(
            False, 1, "stopped\n* stop reason = signal SIGSEGV\nbt all ...")
        r = common.run_under_lldb("/bin/x", _MIN_CONFIG, "lldb", 60, None)
        check("run_under_lldb: crash -> fail with signal name",
              r.outcome == "fail" and r.signal == "SIGSEGV")
        # A crash whose dump ALSO contains an exit-status string must still be a
        # crash, not a pass (the signal check runs first).
        common._capture = _fake_capture(
            False, 1,
            "stop reason = signal SIGABRT\n  Process 1 exited with status = 0")
        r = common.run_under_lldb("/bin/x", _MIN_CONFIG, "lldb", 60, None)
        check("run_under_lldb: embedded status in a crash dump is NOT a pass",
              r.outcome == "fail" and r.signal == "SIGABRT")
        common._capture = _fake_capture(False, 1, "garbage with no markers")
        r = common.run_under_lldb("/bin/x", _MIN_CONFIG, "lldb", 60, None)
        check("run_under_lldb: no markers -> fail",
              r.outcome == "fail" and r.signal == "crash")
        common._capture = _fake_capture(True, None, "partial")
        r = common.run_under_lldb("/bin/x", _MIN_CONFIG, "lldb", 60, None)
        check("run_under_lldb: timeout", r.outcome == "timeout")
    finally:
        common._capture = real


def test_watchdog_should_kill():
    # The pure no-progress decision: kill on a silence gap OR the absolute backstop;
    # spare a run that keeps producing output, however long it legitimately takes.
    decide = common._watchdog_should_kill
    check("watchdog kills on a no-progress gap (a hang)",
          decide(400, 0, 0, 6000, 300))
    check("watchdog spares a slow-but-progressing run",
          not decide(5000, 0, 4990, 6000, 300))
    check("watchdog kills at the absolute backstop despite recent progress",
          decide(6001, 0, 6000, 6000, 300))


class _FakeStream:
    def __init__(self, lines):
        self._lines = list(lines)

    def readline(self):
        return self._lines.pop(0) if self._lines else b""

    def close(self):
        pass


class _FakeProc:
    """A subprocess stand-in for _watch_for_progress: poll() returns None while
    `alive_polls` remain (the run is going), then the exit code."""
    def __init__(self, out, err, alive_polls, returncode=0):
        self.stdout = _FakeStream(out)
        self.stderr = _FakeStream(err)
        self._alive = alive_polls
        self.returncode = returncode

    def poll(self):
        if self._alive > 0:
            self._alive -= 1
            return None
        return self.returncode

    def wait(self):
        self._alive = 0
        return self.returncode


def test_watch_for_progress():
    real_kill = common._kill_process_tree
    killed = []
    common._kill_process_tree = lambda p: killed.append(p)
    try:
        # Clean completion: the process exits, both streams are drained, the exit
        # code passes through, and nothing is killed.
        proc = _FakeProc([b"HEARTBEAT\n", b"RECEIVED=1 SENT=1 EXPECTED=1\n"],
                         [b"(lldb) run\n"], alive_polls=0, returncode=0)
        timed_out, rc, out, err = common._watch_for_progress(
            proc, 6000, 300, poll=lambda: 0.0, sleep=lambda _s: None)
        check("watch_for_progress: clean run is not timed out", timed_out is False)
        check("watch_for_progress: exit code passes through", rc == 0)
        check("watch_for_progress: stdout is drained", "RECEIVED=1" in out)
        check("watch_for_progress: stderr is drained", "lldb" in err)
        check("watch_for_progress: a clean run is not killed", not killed)

        # Hang: the process stays alive and emits nothing -> killed on no-progress,
        # reported as timed out with no returncode. The injected sleep advances the
        # clock past the no-progress window without real waiting.
        clock = [0.0]

        def advance(_s):
            clock[0] += 1000.0

        proc2 = _FakeProc([], [], alive_polls=1000, returncode=0)
        timed_out2, rc2, _o, _e = common._watch_for_progress(
            proc2, 6000, 300, poll=lambda: clock[0], sleep=advance)
        check("watch_for_progress: a hang is timed out", timed_out2 is True)
        check("watch_for_progress: a hung run is killed", len(killed) == 1)
        check("watch_for_progress: a timed-out run has no returncode", rc2 is None)

        # Alive across several poll iterations, but the clock stays inside the
        # no-progress window each step -> the loop runs, the watchdog spares the run,
        # and it completes cleanly (not killed).
        clock3 = [0.0]

        def small_advance(_s):
            clock3[0] += 100.0  # < the 300s no-progress window per step

        proc3 = _FakeProc([b"HEARTBEAT\n"], [], alive_polls=2, returncode=0)
        timed_out3, rc3, _o3, _e3 = common._watch_for_progress(
            proc3, 6000, 300, poll=lambda: clock3[0], sleep=small_advance)
        check("watch_for_progress: an advancing run within the window completes",
              (timed_out3 is False) and (rc3 == 0))
        check("watch_for_progress: an advancing run is not killed", len(killed) == 1)
    finally:
        common._kill_process_tree = real_kill


def test_rlimit_as_supported():
    # Linux honors RLIMIT_AS and ships the resource module: cap applies.
    check("rlimit_as_supported: linux with resource -> True",
          common._rlimit_as_supported("linux", True) is True)
    # macOS has the resource module but setrlimit(RLIMIT_AS) raises there, so
    # the cap must be skipped even when resource is available.
    check("rlimit_as_supported: darwin with resource -> False",
          common._rlimit_as_supported("darwin", True) is False)
    # Windows lacks the resource module (resource_available is False).
    check("rlimit_as_supported: windows without resource -> False",
          common._rlimit_as_supported("win32", False) is False)
    # Allowlist, not denylist: an unvalidated POSIX platform (e.g. a BSD) is
    # skipped even with resource available -- fail safe rather than risk the
    # macOS-style preexec_fn crash on a platform we have not confirmed.
    check("rlimit_as_supported: unvalidated posix with resource -> False",
          common._rlimit_as_supported("freebsd14", True) is False)


def test_bundle_for():
    result = common.RunResult("fail", 1, None, "out", "err")
    limits = {"timeout_seconds": 60, "mem_limit_bytes": None}
    sys_cfg = {"master_seed": 5, "workload": {"seed": 9},
               "runtime": {"ponysystematictestingseed": 42}}
    sys_bundle = common.bundle_for(sys_cfg, "v", "flags", ["argv"], limits, result)
    check("bundle_for: systematic config carries systematic_seed",
          sys_bundle.get("systematic_seed") == 42)
    norm_cfg = {"master_seed": 5, "workload": {"seed": 9}, "runtime": {}}
    norm_bundle = common.bundle_for(norm_cfg, "v", "flags", ["argv"], limits,
                                    result)
    check("bundle_for: normal config omits systematic_seed",
          "systematic_seed" not in norm_bundle)
    check("bundle_for: always carries program_seed",
          norm_bundle["program_seed"] == 9)


def test_resolve_seeds():
    def args(**kw):
        base = {"master_seed": None, "replay": None, "count": None,
                "seeds": None, "start": 1}
        base.update(kw)
        return types.SimpleNamespace(**base)
    check("resolve_seeds: --master-seed wins",
          common.resolve_seeds(args(master_seed=42)) == [42])
    check("resolve_seeds: --replay", common.resolve_seeds(args(replay=7)) == [7])
    check("resolve_seeds: --count from --start",
          common.resolve_seeds(args(count=3, start=10)) == [10, 11, 12])
    check("resolve_seeds: --seeds list",
          common.resolve_seeds(args(seeds="1,2,5")) == [1, 2, 5])
    raised = False
    try:
        common.resolve_seeds(args(seeds="a,b"))
    except SystemExit:
        raised = True
    check("resolve_seeds: bad --seeds dies", raised)


def test_parse_result():
    line = "RECEIVED=136 SENT=136 EXPECTED=136 ORDER_SIG=13652540024091563273"
    parsed = common.parse_result(line)
    check("parse_result: received", parsed.get("received") == "136")
    check("parse_result: sent", parsed.get("sent") == "136")
    check("parse_result: expected", parsed.get("expected") == "136")
    check("parse_result: order_sig",
          parsed.get("order_sig") == "13652540024091563273")
    check("parse_result: empty on garbage",
          common.parse_result("no result here") == {})
    # Partial line (a truncated/timed-out run): present keys parse, missing ones
    # are simply absent (summary_line degrades via .get(..., "?")).
    partial = common.parse_result("RECEIVED=5 SENT=5")
    check("parse_result: partial line keeps present keys, omits missing",
          (partial.get("received") == "5") and ("expected" not in partial))
    # The conservation-failure diag is lowercase (received=/sent=); the success
    # line is uppercase. The lowercase diag must NOT parse as a result -- guards a
    # future case-insensitive tweak from misreading the failure diag as tallies.
    check("parse_result: lowercase conservation diag yields {}",
          common.parse_result("CONSERVATION FAILURE: received=5 sent=6 "
                              "expected=6") == {})


def test_summary_line():
    # summary_line reads DIFFERENT shape fields per kind (mesh/cyclic have
    # chains/ttl, backpressure has producers/messages/apply-every). If its read keys
    # ever drift from resolve_config's emit keys, it raises an unguarded KeyError --
    # at soak time, never in CI -- so exercise every branch here. The shape dicts
    # mirror what the orchestrators emit (see orchestrate_normal.resolve_config and
    # resolve_systematic_workload).
    result = common.RunResult(
        "pass", 0, None,
        "RECEIVED=10 SENT=10 EXPECTED=10 ORDER_SIG=42", "")

    mesh = {"master_seed": 1, "runtime": {},
            "workload": {"seed": 9, "workload": "mesh", "pingers": 8,
                         "chains": 8, "ttl": 16, "payload": "u64",
                         "payload-size": 1, "payload-mode": "forward"}}
    cyclic = {"master_seed": 2, "runtime": {},
              "workload": {"seed": 9, "workload": "cyclic", "generations": 50,
                           "group": 4, "chains": 2, "ttl": 8, "payload": "string",
                           "payload-size": 64, "payload-mode": "fresh"}}
    bp = {"master_seed": 3, "runtime": {},
          "workload": {"seed": 9, "workload": "backpressure", "producers": 64,
                       "messages": 1000, "apply-every": 200, "payload": "string",
                       "payload-size": 64, "payload-mode": "forward"}}
    # A systematic config has NO `workload` key (always mesh by the engine default)
    # and must default to the mesh branch.
    systematic = {"master_seed": 4, "runtime": {},
                  "workload": {"seed": 9, "pingers": 16, "chains": 100, "ttl": 4,
                               "payload": "u64", "payload-size": 8,
                               "payload-mode": "fresh"}}

    # Each kind must render without KeyError and name its kind + its own fields.
    ms = common.summary_line(mesh, result)
    check("summary_line mesh: names kind and pingers/chains",
          ("mesh pingers=8" in ms) and ("chains=8" in ms) and ("PASS" in ms))
    cs = common.summary_line(cyclic, result)
    check("summary_line cyclic: names generations/group/chains",
          ("cyclic generations=50 group=4" in cs) and ("chains=2" in cs))
    bs = common.summary_line(bp, result)
    check("summary_line backpressure: names producers/messages/apply_every",
          ("backpressure producers=64 messages=1000 apply_every=200" in bs)
          and ("chains" not in bs))   # backpressure has no chains/ttl
    ss = common.summary_line(systematic, result)
    check("summary_line defaults a no-workload-key config to mesh",
          "mesh pingers=16" in ss)
    # The detail suffix carries the parsed result on every kind.
    check("summary_line carries the parsed detail",
          "received=10 expected=10 sig=42" in bs)


def main():
    test_derive_seed()
    test_derive_seed_zero_floor()
    test_resolve_systematic_workload_five_field_split()
    test_resolve_systematic_workload_coverage()
    test_draw_swarm_knobs()
    test_draw_payload_mode()
    test_draw_max_threads()
    test_draw_bucketed()
    test_draw_payload()
    test_clamp_ttl()
    test_draw_workload()
    test_build_argv()
    test_run_command()
    test_lldb_argv()
    test_lldb_exit_code()
    test_run_once()
    test_run_under_lldb()
    test_watchdog_should_kill()
    test_watch_for_progress()
    test_rlimit_as_supported()
    test_bundle_for()
    test_resolve_seeds()
    test_parse_result()
    test_summary_line()
    if FAILURES:
        print("%d failure(s): %s" % (len(FAILURES), ", ".join(FAILURES)))
        sys.exit(1)
    print("all stress_common_test checks passed")


if __name__ == "__main__":
    main()
