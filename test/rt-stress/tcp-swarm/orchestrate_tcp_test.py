#!/usr/bin/env python3
"""Unit tests for the pure pieces of orchestrate_tcp.py.

Self-contained (no pytest): `python3 orchestrate_tcp_test.py`, exits 0 on pass /
1 on failure. Picked up by lint-python.yml's `*_test.py` discovery.
"""
import os
import sys

import orchestrate_tcp as o

FAILURES = []


def check(name, condition):
    if condition:
        print("ok   - " + name)
    else:
        print("FAIL - " + name)
        FAILURES.append(name)


def test_resolve_config_golden():
    # Pinned draws: any change to the draw -- a reordered/narrowed/added draw, or a
    # budget/cost-model change -- changes these pinned configs, which is the visible
    # signal that every seed's draw has silently remapped (breaking --replay).
    # Regenerate deliberately if the draw is meant to change. The seeds are chosen to
    # span the draw's branches so a change can't leave all three untouched by luck:
    # seed 0 is a plain writev; seed 6 is write-shape=write AND lands on the memory
    # budget (its connections, messages, and payload are all trimmed to fit, so a
    # cost-model change remaps it, not only a draw-order change); seed 23 exercises the
    # multi-batch writev path (writev-chunks 2048 > IOV_MAX, payload >= chunks) with
    # expect on and a hard close.
    golden0 = {
        "master_seed": 0,
        "runtime": {"ponymaxthreads": 6, "ponynoscale": True},
        "workload": {
            "close": "graceful", "concurrency": 32, "connections": 11236,
            "expect": 0, "messages": 6, "payload-size": 64,
            "read-buffer-size": 65536, "write-shape": "writev",
            "writev-chunks": 64,
            "yield-after-reading": 64, "yield-after-writing": 16384,
        },
    }
    golden6 = {
        "master_seed": 6,
        "runtime": {"ponymaxthreads": 5, "ponynoblock": True},
        "workload": {
            "close": "hard", "concurrency": 256, "connections": 31735,
            "expect": 0, "messages": 1, "payload-size": 256,
            "read-buffer-size": 65536, "write-shape": "write",
            "writev-chunks": 2048,
            "yield-after-reading": 1024, "yield-after-writing": 64,
        },
    }
    golden23 = {
        "master_seed": 23,
        "runtime": {"ponymaxthreads": 6, "ponynoscale": True},
        "workload": {
            "close": "hard", "concurrency": 16, "connections": 1370,
            "expect": 4096, "messages": 7, "payload-size": 4096,
            "read-buffer-size": 65536, "write-shape": "writev",
            "writev-chunks": 2048,
            "yield-after-reading": 1024, "yield-after-writing": 64,
        },
    }
    check("resolve_config(0, 8) matches the pinned draw",
          o.resolve_config(0, 8) == golden0)
    check("resolve_config(6, 8) matches the pinned draw",
          o.resolve_config(6, 8) == golden6)
    check("resolve_config(23, 8) matches the pinned draw",
          o.resolve_config(23, 8) == golden23)
    check("resolve_config is deterministic",
          o.resolve_config(7, 8) == o.resolve_config(7, 8))


def test_clamp_run():
    # Worst-case draw is trimmed under both ceilings, keeping connections + payload.
    c, m = o.clamp_run(100000, 64, 65536)
    check("clamp: worst case stays under the byte ceiling",
          (c * m * 65536) <= o.RUN_MAX_BYTES)
    check("clamp: worst case stays under the exchange ceiling",
          (c * m) <= o.RUN_MAX_EXCHANGES)
    check("clamp: leaves at least one message", m >= 1)
    # A 64 KiB worst case can't keep all 100k connections under the byte ceiling
    # (100k * 64 KiB > 4 GB even at one message), so connections is trimmed as a
    # last resort -- to exactly the largest count that fits the byte ceiling, never
    # below the MIN_CONNECTIONS floor.
    expected_c = max(o.MIN_CONNECTIONS, o.RUN_MAX_BYTES // (m * 65536))
    check("clamp: trims connections to the largest that fits the byte ceiling",
          (c == expected_c) and (c >= o.MIN_CONNECTIONS))
    # When connections * payload fits under the ceiling, connections is kept and
    # only messages are trimmed.
    ck_c, _ = o.clamp_run(50000, 64, 1024)
    check("clamp: keeps connections when they fit under the ceiling",
          ck_c == 50000)
    # A small config is left untouched.
    check("clamp: small config unchanged", o.clamp_run(500, 2, 64) == (500, 2))
    # The MIN_CONNECTIONS floor holds even when the byte ceiling would trim below
    # it (a pathological payload no real draw produces, but the guard must hold).
    huge_payload = o.RUN_MAX_BYTES  # one such connection already blows the ceiling
    fc, fm = o.clamp_run(100000, 1, huge_payload)
    check("clamp: never trims connections below the MIN_CONNECTIONS floor",
          fc == o.MIN_CONNECTIONS and fm >= 1)
    # Every drawn seed is under both ceilings after the clamp.
    over = 0
    for seed in range(1000):
        w = o.resolve_config(seed, 8)["workload"]
        c, m, p = w["connections"], w["messages"], max(1, w["payload-size"])
        if (c * m > o.RUN_MAX_EXCHANGES) or (c * m * p > o.RUN_MAX_BYTES):
            over += 1
    check("clamp: no drawn seed exceeds the ceilings", over == 0)


def test_memory_budget():
    # No drawn config may exceed the memory budget. A draw over the RLIMIT_AS cap gets
    # killed by the runtime's own out-of-memory abort -- a false failure that reads
    # like a runtime crash. Also confirm the budget isn't vacuous: some draws land near
    # the ceiling, so it is a tight bound, not a ceiling nothing reaches.
    over = 0
    binds = 0
    for seed in range(5000):
        w = o.resolve_config(seed, 8)["workload"]
        peak = o.est_peak_bytes(
            w["concurrency"], w["messages"], w["payload-size"],
            w["write-shape"] == "writev", w["writev-chunks"],
            w["read-buffer-size"], w["connections"])
        if peak > o.MEM_BUDGET_BYTES:
            over += 1
        if peak > (o.MEM_BUDGET_BYTES * 9) // 10:
            binds += 1
    check("memory: no drawn config exceeds the budget", over == 0)
    check("memory: the budget binds (some draws land near the ceiling)", binds > 0)


def test_memory_budget_trims():
    # The fit helpers -- the building block of the rotating draw -- trim a value that
    # would break the budget down to the largest one that fits (never below the
    # minimum) and leave a fitting value alone.
    mins = {"connections": o.MIN_CONNECTIONS, "concurrency": min(o.CONCURRENCY),
            "messages": o.MESSAGE_BUCKETS["small"][0], "payload": min(o.PAYLOAD_SIZES),
            "writev_chunks": min(o.WRITEV_CHUNKS),
            "read_buffer": min(o.READ_BUFFER_SIZES), "use_writev": True}
    check("fit_discrete: a fitting value passes through unchanged",
          o._fit_discrete("read_buffer", 65536, mins, o.READ_BUFFER_SIZES) == 65536)
    # With connections at the max, a 64 KiB read buffer blows the budget -> trimmed to
    # a smaller listed value. Boundary: the trimmed value fits and the NEXT larger
    # listed value would not (so the helper returns the largest that fits, not any).
    loaded = dict(mins, connections=100000)
    trimmed = o._fit_discrete("read_buffer", 65536, loaded, o.READ_BUFFER_SIZES)
    nxt = o.READ_BUFFER_SIZES[o.READ_BUFFER_SIZES.index(trimmed) + 1]
    check("fit_discrete: trims to the largest listed value that fits (next would not)",
          trimmed < 65536
          and o.est_peak_bytes(**dict(loaded, read_buffer=trimmed))
          <= o.MEM_BUDGET_BYTES < o.est_peak_bytes(**dict(loaded, read_buffer=nxt)))
    # Continuous: a huge connection count is trimmed when the read buffer is large,
    # never below the MIN_CONNECTIONS floor. Boundary: fc fits and fc+1 does not, so
    # the binary search returns the exact largest fitting integer (catches an off-by-one
    # in the comparison).
    big_rb = dict(mins, read_buffer=65536)
    fc = o._fit_continuous("connections", 100000, big_rb, o.MIN_CONNECTIONS)
    check("fit_continuous: trims to the exact budget boundary (fc fits, fc+1 doesn't)",
          o.MIN_CONNECTIONS <= fc < 100000
          and o.est_peak_bytes(**dict(big_rb, connections=fc))
          <= o.MEM_BUDGET_BYTES < o.est_peak_bytes(**dict(big_rb, connections=fc + 1)))


def test_est_peak_bytes_monotonic():
    # The fit helpers' binary search / list scan are correct only if est_peak_bytes is
    # non-decreasing in the swept lever. Guard that: a future non-monotonic cost term
    # would break the search silently. Sweep each lever across its domain with the
    # others fixed at a mid value.
    base = {"connections": 10000, "concurrency": 32, "messages": 8, "payload": 1024,
            "writev_chunks": 64, "read_buffer": 16384, "use_writev": True}
    domains = {"connections": range(100, 100001, 2500),
               "concurrency": o.CONCURRENCY,
               "messages": range(1, 65),
               "payload": o.PAYLOAD_SIZES,
               "writev_chunks": o.WRITEV_CHUNKS,
               "read_buffer": o.READ_BUFFER_SIZES}
    non_monotone = []
    for lever, values in domains.items():
        prev = None
        for v in values:
            e = o.est_peak_bytes(**dict(base, **{lever: v}))
            if prev is not None and e < prev:
                non_monotone.append(lever)
            prev = e
    check("est_peak_bytes is non-decreasing in every lever: "
          + (", ".join(sorted(set(non_monotone))) if non_monotone else "all monotone"),
          not non_monotone)


def test_memory_budget_rotates_the_trimmed_lever():
    # Two properties, together the rotation:
    #   (a) every memory lever reaches its largest value on some seed -- so a large
    #       draw is never impossible for any of them (the swarm stays a swarm).
    #   (b) every memory lever is trimmed below its rolled value on some seed.
    # (b) is the rotation, and it holds for all six levers ONLY because the draw order
    # is shuffled per seed. Under a fixed order only the LAST levers drawn ever trim --
    # connections/concurrency/messages/payload would never trim -- so replacing the
    # shuffle with identity fails this test. Trims are observed by wrapping the two fit
    # helpers (they are where a value gets clamped down); the wrappers consume no rng,
    # so the draw is unchanged.
    levers = ("connections", "concurrency", "messages", "payload",
              "writev_chunks", "read_buffer")
    reached = {k: False for k in levers}
    trimmed = {k: False for k in levers}
    orig_d, orig_c = o._fit_discrete, o._fit_continuous

    def wrap_d(key, drawn, chosen, values):
        got = orig_d(key, drawn, chosen, values)
        if got != drawn:
            trimmed[key] = True
        return got

    def wrap_c(key, drawn, chosen, floor):
        got = orig_c(key, drawn, chosen, floor)
        if got != drawn:
            trimmed[key] = True
        return got

    o._fit_discrete, o._fit_continuous = wrap_d, wrap_c
    try:
        for seed in range(4000):
            w = o.resolve_config(seed, 8)["workload"]
            if w["concurrency"] == max(o.CONCURRENCY):
                reached["concurrency"] = True
            if w["payload-size"] == max(o.PAYLOAD_SIZES):
                reached["payload"] = True
            if w["writev-chunks"] == max(o.WRITEV_CHUNKS):
                reached["writev_chunks"] = True
            if w["read-buffer-size"] == max(o.READ_BUFFER_SIZES):
                reached["read_buffer"] = True
            if w["connections"] > o.CONNECTION_BUCKETS["medium"][1]:
                reached["connections"] = True   # into the large connection bucket
            if w["messages"] == o.MESSAGE_BUCKETS["large"][1]:
                reached["messages"] = True
    finally:
        o._fit_discrete, o._fit_continuous = orig_d, orig_c
    never_large = [k for k, v in reached.items() if not v]
    never_trimmed = [k for k, v in trimmed.items() if not v]
    check("memory: every lever reaches large on some seed: "
          + (", ".join(never_large) + " never do" if never_large else "all do"),
          not never_large)
    check("memory: every lever is trimmed on some seed (the shuffle rotates the "
          "trim): " + (", ".join(never_trimmed) + " never trim" if never_trimmed
                       else "all trim"),
          not never_trimmed)


def test_resolve_config_coverage_and_invariants():
    write_shapes = set()
    closes = set()
    expect_states = set()
    chunk_vals = set()
    multibatch_seeds = 0
    yield_fires_seeds = 0
    noscale = pin = pinasio = noblock = 0
    invariants = True
    for seed in range(500):
        c = o.resolve_config(seed, 8)
        w, r = c["workload"], c["runtime"]
        write_shapes.add(w["write-shape"])
        closes.add(w["close"])
        expect_states.add(w["expect"] > 0)
        chunk_vals.add(w["writev-chunks"])
        # A writev whose non-empty buffer count exceeds POSIX IOV_MAX (1024)
        # reaches TCPConnection's multi-batch send path. That needs writev,
        # writev-chunks > 1024, and payload >= chunks (so no buffer is empty). The
        # mid-write yield fires only on that path, and only once one IOV_MAX-buffer
        # batch's bytes reach --yield-after-writing. Since the yield is the whole
        # point of the writev-chunks lever, both must stay reachable or those paths
        # go untested. (IOV_MAX here is the POSIX value; see
        # .known-couplings/tcp-swarm-writev-chunks-iov-max.md.)
        iov_max = 1024
        chunks = w["writev-chunks"]
        if (w["write-shape"] == "writev" and chunks > iov_max
                and w["payload-size"] >= chunks):
            multibatch_seeds += 1
            first_batch_bytes = iov_max * (w["payload-size"] // chunks)
            if first_batch_bytes >= w["yield-after-writing"]:
                yield_fires_seeds += 1
        noscale += 1 if r.get("ponynoscale") else 0
        pin += 1 if r.get("ponypin") else 0
        pinasio += 1 if r.get("ponypinasio") else 0
        noblock += 1 if r.get("ponynoblock") else 0
        # Invariants the engine relies on.
        if w["expect"] > w["read-buffer-size"]:
            invariants = False           # expect() errors above the read buffer
        if w["expect"] not in (0, min(w["payload-size"], w["read-buffer-size"])):
            invariants = False
        # Framed reads must consume a whole number of frames, or the trailing
        # partial frame is withheld forever and the connection never completes
        # (a hang). expect divides payload (all sizes are powers of two), so this
        # holds by construction -- assert it so a future size change that breaks
        # it fails here instead of hanging a CI run.
        if w["expect"] > 0 and (w["payload-size"] * w["messages"]) % w["expect"]:
            invariants = False
        if w["concurrency"] not in o.CONCURRENCY:
            invariants = False
        if w["payload-size"] not in o.PAYLOAD_SIZES:
            invariants = False
        if w["read-buffer-size"] not in o.READ_BUFFER_SIZES:
            invariants = False
        if w["write-shape"] not in o.WRITE_SHAPES:
            invariants = False
        if w["writev-chunks"] not in o.WRITEV_CHUNKS:
            invariants = False
        if w["close"] not in o.CLOSE_KINDS:
            invariants = False
        if not (1 <= r["ponymaxthreads"] <= 8):
            invariants = False
        clo, chi = o.CONNECTION_BUCKETS["small"][0], o.CONNECTION_BUCKETS["large"][1]
        if not (clo <= w["connections"] <= chi):
            invariants = False
        if r.get("ponypinasio") and not r.get("ponypin"):
            invariants = False           # pinasio requires pin
    check("both write shapes appear", write_shapes == {"write", "writev"})
    # Hardcoded literal (not set(o.WRITEV_CHUNKS)) so narrowing the constant is
    # caught here, matching the sibling write-shape/close checks.
    check("all writev-chunk counts appear", chunk_vals == {4, 64, 2048})
    check("multi-batch writev is reachable by the draw", multibatch_seeds > 0)
    check("mid-write yield is reachable via writev-chunks", yield_fires_seeds > 0)
    check("both close kinds appear", closes == {"graceful", "hard"})
    check("expect appears both on and off", expect_states == {True, False})
    check("ponynoscale is drawn sometimes and not always",
          0 < noscale < 500)
    check("ponypin and ponypinasio both appear", pin > 0 and pinasio > 0)
    check("ponynoblock is drawn sometimes and not always",
          0 < noblock < 500)
    check("per-seed invariants hold across 500 seeds", invariants)


def test_max_connections_cap():
    # The cap (Windows CI) only lowers connections to min(drawn, cap) and touches
    # nothing else in the draw. Also confirm it isn't vacuous -- some seeds draw
    # more than the cap.
    ok = True
    capped_any = False
    for seed in range(300):
        base = o.resolve_config(seed, 8)
        capped = o.resolve_config(seed, 8, max_connections=1000)
        if capped["workload"]["connections"] != min(
                base["workload"]["connections"], 1000):
            ok = False
        if base["workload"]["connections"] > 1000:
            capped_any = True
        b = {k: v for k, v in base["workload"].items() if k != "connections"}
        c = {k: v for k, v in capped["workload"].items() if k != "connections"}
        if (b != c) or (base["runtime"] != capped["runtime"]):
            ok = False
    check("max_connections: caps to min(drawn, cap), nothing else moves", ok)
    check("max_connections: actually caps some seeds (not vacuous)", capped_any)


def test_ponymaxthreads_is_last_and_host_dependent():
    # Everything but --ponymaxthreads must be identical across host core counts;
    # only the last draw (ponymaxthreads) may differ. This is what lets the draw
    # order stay stable across hosts.
    stable = True
    differed = 0
    bounded_low = True
    exceeded_low = False
    for seed in range(200):
        a = o.resolve_config(seed, 4)
        b = o.resolve_config(seed, 64)
        if a["workload"] != b["workload"]:
            stable = False
        ra = dict(a["runtime"])
        rb = dict(b["runtime"])
        ta = ra.pop("ponymaxthreads", None)
        tb = rb.pop("ponymaxthreads", None)
        if ra != rb:
            stable = False
        if ta != tb:
            differed += 1
        if not (1 <= ta <= 4):
            bounded_low = False          # bounded by the smaller core count
        if tb > 4:
            exceeded_low = True          # the larger count reaches higher
    check("only ponymaxthreads varies with the core count", stable)
    # The host-dependence isn't vacuous: ponymaxthreads is bounded by the core
    # count, and a larger count actually produces larger draws for some seeds.
    check("ponymaxthreads is bounded by the (smaller) core count", bounded_low)
    check("ponymaxthreads genuinely differs across core counts",
          differed > 0 and exceeded_low)


def test_build_argv():
    config = {
        "master_seed": 0,
        "workload": {"connections": 10, "payload-size": 64},
        "runtime": {"ponynoscale": True, "ponymaxthreads": 3},
    }
    argv = o.build_argv("/bin/tcp_swarm", config)
    check("build_argv: binary first", argv[0] == "/bin/tcp_swarm")
    check("build_argv: workload as --key value",
          "--connections" in argv and argv[argv.index("--connections") + 1] == "10")
    check("build_argv: True flag is bare", "--ponynoscale" in argv
          and "True" not in argv)
    check("build_argv: valued runtime flag",
          argv[argv.index("--ponymaxthreads") + 1] == "3")


def test_parse_result():
    out = ("RESULT connections=300 spawned=300 completed=298 failed=2 "
           "verified=298 mismatched=0\nPASS")
    parsed = o.parse_result(out)
    check("parse_result: reads the tally",
          parsed == {"connections": 300, "spawned": 300, "completed": 298,
                     "failed": 2, "verified": 298, "mismatched": 0})
    check("parse_result: empty on garbage", o.parse_result("nothing here") == {})
    # The FAIL line repeats `connect_failed=`. `failed=` must match the key as a
    # whole token, not as the tail of `connect_failed=`. This adversarial string
    # puts a differing `connect_failed=` FIRST so an unanchored match would read
    # the wrong number -- the \b anchor makes it skip to the real `failed=`.
    adversarial = "connect_failed=99 verified=288 failed=10 mismatched=2"
    pf = o.parse_result(adversarial)
    check("parse_result: failed matches the key, not connect_failed",
          pf["failed"] == 10)


def test_lldb_argv():
    argv = o.lldb_argv("/usr/bin/lldb", ["/bin/tcp_swarm", "--connections", "1"])
    joined = " ".join(argv)
    check("lldb_argv: --batch present", "--batch" in argv)
    check("lldb_argv: captures a backtrace on crash", "bt all" in argv)
    sep = argv.index("--")
    check("lldb_argv: engine argv follows --",
          argv[sep + 1:] == ["/bin/tcp_swarm", "--connections", "1"])
    if os.name == "posix":
        check("lldb_argv (posix): passes SIGUSR2 through",
              "process handle SIGUSR2 --pass true --stop false" in joined)


def test_lldb_exit_code():
    out = "Process 12345 exited with status = 0 (0x00000000)"
    check("lldb_exit_code: reads a clean exit", o.lldb_exit_code(out) == 0)
    check("lldb_exit_code: reads a non-zero exit",
          o.lldb_exit_code("Process 9 exited with status = 1") == 1)
    check("lldb_exit_code: None when the line is absent",
          o.lldb_exit_code("no exit line here") is None)


def test_watchdog_kill_reason():
    # No-progress: the completed count last advanced longer ago than the window --
    # a hang.
    check("watchdog: no_progress when done stalls past the window",
          o._watchdog_kill_reason(now=1000, start=0, last_progress=699,
                                  timeout=6000, no_progress_seconds=300)
          == "no_progress")
    # Backstop: progress is fresh, but the run has passed the absolute cap -- a
    # healthy over-long run, stopped but not failed.
    check("watchdog: backstop when still advancing past the cap",
          o._watchdog_kill_reason(now=7000, start=0, last_progress=6900,
                                  timeout=6000, no_progress_seconds=300)
          == "backstop")
    # Healthy: recent progress, well inside the cap.
    check("watchdog: None when progressing inside the cap",
          o._watchdog_kill_reason(now=100, start=0, last_progress=100,
                                  timeout=6000, no_progress_seconds=300) is None)
    # A stall that is also past the cap is a hang, not a backstop: no_progress is
    # checked first, so a stuck run past the cap still fails.
    check("watchdog: a stall past the cap is a hang, not a backstop",
          o._watchdog_kill_reason(now=7000, start=0, last_progress=6000,
                                  timeout=6000, no_progress_seconds=300)
          == "no_progress")
    # Boundaries: the checks are strict `>`, so a gap or elapsed time exactly at the
    # threshold does NOT fire; one second past does.
    check("watchdog: a gap exactly at the window does not fire",
          o._watchdog_kill_reason(now=300, start=0, last_progress=0,
                                  timeout=6000, no_progress_seconds=300) is None)
    check("watchdog: one past the window fires no_progress",
          o._watchdog_kill_reason(now=301, start=0, last_progress=0,
                                  timeout=6000, no_progress_seconds=300)
          == "no_progress")
    check("watchdog: elapsed exactly at the backstop does not fire",
          o._watchdog_kill_reason(now=6000, start=0, last_progress=6000,
                                  timeout=6000, no_progress_seconds=300) is None)
    check("watchdog: one past the backstop fires backstop",
          o._watchdog_kill_reason(now=6001, start=0, last_progress=6001,
                                  timeout=6000, no_progress_seconds=300)
          == "backstop")


def test_parse_done():
    # A heartbeat line yields its completed count; anything else yields None.
    check("parse_done: reads the count from a heartbeat",
          o._parse_done(b"HEARTBEAT done=1234 of 44688\n") == 1234)
    check("parse_done: None on the RESULT line",
          o._parse_done(b"RESULT connections=500 completed=500\n") is None)
    check("parse_done: None on a bare done= without the HEARTBEAT prefix",
          o._parse_done(b"done=5 but not a heartbeat\n") is None)


def test_is_progress():
    # The discriminator: progress is the completed count EXCEEDING the max seen, not
    # any output. A frozen count (heartbeats still printing) or an absent count is not
    # progress -- which is how a streaming-but-stalled run is caught as a hang.
    check("is_progress: the first count seen is progress (from the -1 sentinel)",
          o._is_progress(0, -1))
    check("is_progress: a rising count is progress", o._is_progress(6, 5))
    check("is_progress: a frozen count is not progress", not o._is_progress(5, 5))
    check("is_progress: a lower count is not progress", not o._is_progress(3, 5))
    check("is_progress: a non-heartbeat line (None) is not progress",
          not o._is_progress(None, 5))


def test_classify_outcome():
    check("classify: no_progress -> hang",
          o._classify_outcome("no_progress", None) == "hang")
    check("classify: backstop -> incomplete",
          o._classify_outcome("backstop", None) == "incomplete")
    check("classify: clean exit 0 -> pass",
          o._classify_outcome(None, 0) == "pass")
    check("classify: clean non-zero exit -> fail",
          o._classify_outcome(None, 1) == "fail")
    check("classify: signal death (negative rc) -> fail",
          o._classify_outcome(None, -11) == "fail")


def test_is_failure():
    check("is_failure: fail is a failure", o._is_failure("fail"))
    check("is_failure: hang is a failure", o._is_failure("hang"))
    check("is_failure: pass is not a failure", not o._is_failure("pass"))
    check("is_failure: incomplete is not a failure",
          not o._is_failure("incomplete"))


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
    # Progress is the HEARTBEAT `done=` count RISING, not raw output -- so a run that
    # keeps printing heartbeats with a frozen count is a hang, and one whose count
    # rises is spared. Drive _watch_for_progress with a fake process and an injected
    # clock (no real waiting). _kill_process_tree is stubbed to just record the kill.
    real_kill = o._kill_process_tree
    killed = []
    o._kill_process_tree = lambda p: killed.append(p)
    try:
        # Clean completion: the process exits, both streams drain, the exit code
        # passes through with no kill reason, and nothing is killed.
        proc = _FakeProc([b"HEARTBEAT done=1 of 10\n", b"RESULT connections=10\n"],
                         [b"(lldb) run\n"], alive_polls=0, returncode=0)
        reason, rc, out, err = o._watch_for_progress(
            proc, 6000, 300, poll=lambda: 0.0, sleep=lambda _s: None)
        check("watch: a clean run has no kill reason", reason is None)
        check("watch: a clean run's exit code passes through", rc == 0)
        check("watch: a clean run drains stdout", "RESULT" in out)
        check("watch: a clean run drains stderr", "lldb" in err)
        check("watch: a clean run is not killed", not killed)

        # A run that keeps printing heartbeats but whose `done` count never rises past
        # its first value is a hang: the injected sleep runs the clock past the
        # no-progress window and it is killed. This exercises the threaded kill path
        # and the return contract; the done-vs-output discriminator itself is pinned by
        # test_is_progress, since this fake delivers all lines at clock 0.
        clock = [0.0]

        def advance(_s):
            clock[0] += 1000.0

        frozen = [b"HEARTBEAT done=5 of 10\n"] * 4
        proc2 = _FakeProc(frozen, [], alive_polls=1000, returncode=0)
        reason2, rc2, _o, _e = o._watch_for_progress(
            proc2, 6000, 300, poll=lambda: clock[0], sleep=advance)
        check("watch: heartbeats with a frozen done count is a hang",
              reason2 == "no_progress")
        check("watch: a hung run is killed", len(killed) == 1)
        check("watch: a hung run has no returncode", rc2 is None)

        # A run whose clock stays inside the no-progress window each step is spared and
        # completes cleanly (no kill reason) -- the watchdog kills only on a gap past
        # the window or on the backstop.
        clock3 = [0.0]

        def small_advance(_s):
            clock3[0] += 100.0  # < the 300s window per step

        proc3 = _FakeProc([b"HEARTBEAT done=1 of 10\n"], [], alive_polls=2,
                          returncode=0)
        reason3, rc3, _o3, _e3 = o._watch_for_progress(
            proc3, 6000, 300, poll=lambda: clock3[0], sleep=small_advance)
        check("watch: a run advancing inside the window completes",
              (reason3 is None) and (rc3 == 0))
        check("watch: a spared run is not killed", len(killed) == 1)
    finally:
        o._kill_process_tree = real_kill


_MIN_CONFIG = {"master_seed": 0, "workload": {"connections": 1}, "runtime": {}}


def _fake_capture(kill_reason, returncode, stdout, stderr=""):
    """A stand-in for o._capture that returns crafted output, so the run classifiers
    can be tested without launching a process."""
    return lambda *a, **k: (kill_reason, returncode, stdout, stderr)


def test_run_once():
    real = o._capture
    try:
        o._capture = _fake_capture(None, 0, "ok")
        r = o.run_once("/bin/x", _MIN_CONFIG, 6000, None, 300)
        check("run_once: clean exit 0 -> pass",
              r.outcome == "pass" and r.returncode == 0 and r.signal is None)
        o._capture = _fake_capture(None, 1, "", "mismatch")
        r = o.run_once("/bin/x", _MIN_CONFIG, 6000, None, 300)
        check("run_once: clean exit 1 -> fail",
              r.outcome == "fail" and r.returncode == 1)
        o._capture = _fake_capture(None, -11, "", "")
        r = o.run_once("/bin/x", _MIN_CONFIG, 6000, None, 300)
        check("run_once: a negative code -> fail with signal number 11",
              r.outcome == "fail" and r.signal == 11)
        o._capture = _fake_capture("no_progress", None, "", "")
        r = o.run_once("/bin/x", _MIN_CONFIG, 6000, None, 300)
        check("run_once: a no-progress kill -> hang (a failure)",
              r.outcome == "hang" and o._is_failure(r.outcome))
        o._capture = _fake_capture("backstop", None, "", "")
        r = o.run_once("/bin/x", _MIN_CONFIG, 6000, None, 300)
        check("run_once: a backstop kill -> incomplete (NOT a failure)",
              r.outcome == "incomplete" and not o._is_failure(r.outcome))
    finally:
        o._capture = real


def test_run_under_lldb():
    # The load-bearing classification under lldb (the path CI actually runs): a kill
    # reason maps to hang/incomplete ahead of any crash parsing; a clean exit is
    # classified from the lldb crash-signal name or the exit-status line.
    real = o._capture
    try:
        o._capture = _fake_capture(None, 0, "Process 7 exited with status = 0 (0x0)")
        r = o.run_under_lldb("/bin/x", "lldb", _MIN_CONFIG, 6000, None, 300)
        check("run_under_lldb: clean exit 0 -> pass",
              r.outcome == "pass" and r.returncode == 0)
        o._capture = _fake_capture(None, 0, "Process 7 exited with status = 1 (0x1)")
        r = o.run_under_lldb("/bin/x", "lldb", _MIN_CONFIG, 6000, None, 300)
        check("run_under_lldb: exit 1 -> fail",
              r.outcome == "fail" and r.returncode == 1)
        o._capture = _fake_capture(
            None, 1, "stopped\n* stop reason = signal SIGSEGV\nbt all ...")
        r = o.run_under_lldb("/bin/x", "lldb", _MIN_CONFIG, 6000, None, 300)
        check("run_under_lldb: a crash -> fail with the signal name",
              r.outcome == "fail" and r.signal == "SIGSEGV")
        # A crash whose dump ALSO contains an exit-status line must still be a crash,
        # not a pass -- the signal check runs first.
        o._capture = _fake_capture(
            None, 1,
            "stop reason = signal SIGABRT\n  Process 1 exited with status = 0")
        r = o.run_under_lldb("/bin/x", "lldb", _MIN_CONFIG, 6000, None, 300)
        check("run_under_lldb: an embedded status in a crash dump is not a pass",
              r.outcome == "fail" and r.signal == "SIGABRT")
        o._capture = _fake_capture(None, 1, "garbage with no markers")
        r = o.run_under_lldb("/bin/x", "lldb", _MIN_CONFIG, 6000, None, 300)
        check("run_under_lldb: no markers -> fail",
              r.outcome == "fail" and r.signal == "crash")
        o._capture = _fake_capture("no_progress", None, "partial")
        r = o.run_under_lldb("/bin/x", "lldb", _MIN_CONFIG, 6000, None, 300)
        check("run_under_lldb: a no-progress kill -> hang (a failure)",
              r.outcome == "hang" and o._is_failure(r.outcome))
        o._capture = _fake_capture("backstop", None, "partial")
        r = o.run_under_lldb("/bin/x", "lldb", _MIN_CONFIG, 6000, None, 300)
        check("run_under_lldb: a backstop kill -> incomplete (NOT a failure)",
              r.outcome == "incomplete" and not o._is_failure(r.outcome))
    finally:
        o._capture = real


def test_rlimit_as_supported():
    # Only Linux honors the RLIMIT_AS preexec cap (macOS raises in preexec,
    # Windows lacks the resource module).
    check("rlimit: on for linux with resource",
          o._rlimit_as_supported("linux", True))
    check("rlimit: off for macOS", not o._rlimit_as_supported("darwin", True))
    check("rlimit: off for windows", not o._rlimit_as_supported("win32", True))
    check("rlimit: off when the resource module is absent",
          not o._rlimit_as_supported("linux", False))


def main():
    for fn in (test_resolve_config_golden, test_clamp_run,
               test_memory_budget, test_memory_budget_trims,
               test_est_peak_bytes_monotonic,
               test_memory_budget_rotates_the_trimmed_lever,
               test_max_connections_cap,
               test_resolve_config_coverage_and_invariants,
               test_ponymaxthreads_is_last_and_host_dependent,
               test_build_argv, test_parse_result, test_lldb_argv,
               test_lldb_exit_code, test_watchdog_kill_reason,
               test_parse_done, test_is_progress, test_classify_outcome,
               test_is_failure, test_watch_for_progress, test_run_once,
               test_run_under_lldb, test_rlimit_as_supported):
        fn()
    if FAILURES:
        print("\n%d failure(s): %s" % (len(FAILURES), ", ".join(FAILURES)))
        return 1
    print("\nall orchestrate_tcp_test checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
