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
    # Pinned draws: a reordered/narrowed/added draw changes these and would
    # silently remap every seed (breaking --replay). Regenerate deliberately if
    # the draw is intended to change. Three seeds spanning both write shapes, so a
    # change is unlikely to leave all untouched by luck AND a regression that
    # touched only one write-shape branch can't hide. Seed 0 is a plain writev;
    # seed 5 is write-shape=write (payload 1, hard close); seed 16 exercises the
    # multi-batch writev path (writev, writev-chunks 2048 > IOV_MAX, payload >=
    # chunks) plus expect > 0 and a hard close.
    golden0 = {
        "master_seed": 0,
        "runtime": {"ponymaxthreads": 5, "ponypin": True},
        "workload": {
            "close": "graceful", "concurrency": 8, "connections": 75126,
            "expect": 0, "messages": 26, "payload-size": 1024,
            "read-buffer-size": 65536, "write-shape": "writev",
            "writev-chunks": 64,
            "yield-after-reading": 1024, "yield-after-writing": 16384,
        },
    }
    golden5 = {
        "master_seed": 5,
        "runtime": {"ponymaxthreads": 4, "ponynoblock": True},
        "workload": {
            "close": "hard", "concurrency": 256, "connections": 13749,
            "expect": 0, "messages": 32, "payload-size": 1,
            "read-buffer-size": 128, "write-shape": "write",
            "writev-chunks": 4,
            "yield-after-reading": 1024, "yield-after-writing": 1024,
        },
    }
    golden16 = {
        "master_seed": 16,
        "runtime": {"ponymaxthreads": 5, "ponynoblock": True,
                    "ponynoscale": True},
        "workload": {
            "close": "hard", "concurrency": 32, "connections": 17745,
            "expect": 16384, "messages": 1, "payload-size": 16384,
            "read-buffer-size": 16384, "write-shape": "writev",
            "writev-chunks": 2048,
            "yield-after-reading": 64, "yield-after-writing": 16384,
        },
    }
    check("resolve_config(0, 8) matches the pinned draw",
          o.resolve_config(0, 8) == golden0)
    check("resolve_config(5, 8) matches the pinned draw",
          o.resolve_config(5, 8) == golden5)
    check("resolve_config(16, 8) matches the pinned draw",
          o.resolve_config(16, 8) == golden16)
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


def test_watchdog_should_kill():
    # No-progress: the last output was longer ago than the no-progress window.
    check("watchdog: kills on a no-progress gap",
          o._watchdog_should_kill(now=1000, start=0, last_output=699,
                                  timeout=6000, no_progress_seconds=300))
    # Absolute timeout: output is fresh, but the run has exceeded --timeout.
    check("watchdog: kills on the absolute timeout",
          o._watchdog_should_kill(now=7000, start=0, last_output=7000,
                                  timeout=6000, no_progress_seconds=300))
    # Healthy: recent output and well inside the timeout.
    check("watchdog: lets a healthy run continue",
          not o._watchdog_should_kill(now=100, start=0, last_output=100,
                                      timeout=6000, no_progress_seconds=300))


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
               test_max_connections_cap,
               test_resolve_config_coverage_and_invariants,
               test_ponymaxthreads_is_last_and_host_dependent,
               test_build_argv, test_parse_result, test_lldb_argv,
               test_lldb_exit_code, test_watchdog_should_kill,
               test_rlimit_as_supported):
        fn()
    if FAILURES:
        print("\n%d failure(s): %s" % (len(FAILURES), ", ".join(FAILURES)))
        return 1
    print("\nall orchestrate_tcp_test checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
