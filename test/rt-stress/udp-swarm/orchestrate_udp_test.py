#!/usr/bin/env python3
"""Unit tests for the pure pieces of orchestrate_udp.py.

Self-contained (no pytest): `python3 orchestrate_udp_test.py`, exits 0 on pass /
1 on failure.
"""
import os
import sys

import orchestrate_udp as o

FAILURES = []


def check(name, condition):
    if condition:
        print("ok   - " + name)
    else:
        print("FAIL - " + name)
        FAILURES.append(name)


def test_resolve_config_deterministic():
    check("resolve_config is deterministic",
          o.resolve_config(7, 8) == o.resolve_config(7, 8))


def test_clamp_run():
    c, d = o.clamp_run(32, 100000, 8192)
    check("clamp: stays under the round-trip ceiling",
          (c * d) <= o.RUN_MAX_ROUND_TRIPS)
    check("clamp: stays under the byte ceiling",
          (c * d * 8192) <= o.RUN_MAX_BYTES)
    check("clamp: leaves at least MIN_DATAGRAMS datagrams", d >= o.MIN_DATAGRAMS)
    # A small config is left untouched.
    check("clamp: small config unchanged", o.clamp_run(2, 50, 64) == (2, 50))
    # Pathological input exercises the last-resort client-trimming path.
    c3, d3 = o.clamp_run(30000, 100000, 8192)
    check("clamp: pathological input stays under ceilings",
          (c3 * d3 <= o.RUN_MAX_ROUND_TRIPS)
          and (c3 * d3 * 8192 <= o.RUN_MAX_BYTES))
    check("clamp: pathological input keeps at least 1 client", c3 >= 1)
    # Every drawn seed is under all ceilings after the clamp.
    over = 0
    burst_over = 0
    for seed in range(1000):
        w = o.resolve_config(seed, 64)["workload"]
        c, d, p = w["clients"], w["datagrams"], max(1, w["payload-size"])
        b = w["batch-size"]
        if (c * d > o.RUN_MAX_ROUND_TRIPS) or (c * d * p > o.RUN_MAX_BYTES):
            over += 1
        if (c * b * p > o.MAX_BURST_BYTES) or (c * b > o.MAX_BURST_DATAGRAMS):
            burst_over += 1
    check("clamp: no drawn seed exceeds the ceilings", over == 0)
    check("clamp: no drawn seed exceeds the burst ceiling", burst_over == 0)


def test_resolve_config_coverage():
    # Use max_threads=64 so the thread/client clamp does not mask profile
    # coverage.  The burst clamp still reduces clients for large payloads,
    # but every profile client value survives for at least some payloads.
    payloads = set()
    batches = set()
    clients = set()
    rbufs = set()
    max_dgs = set()
    noscale = pin = pinasio = noblock = 0
    invariants = True
    for seed in range(500):
        c = o.resolve_config(seed, 64)
        w, r = c["workload"], c["runtime"]
        payloads.add(w["payload-size"])
        batches.add(w["batch-size"])
        clients.add(w["clients"])
        rbufs.add(w["read-buffer-size"])
        max_dgs.add(w["max-datagrams-per-turn"])
        noscale += 1 if r.get("ponynoscale") else 0
        pin += 1 if r.get("ponypin") else 0
        pinasio += 1 if r.get("ponypinasio") else 0
        noblock += 1 if r.get("ponynoblock") else 0
        if w["read-buffer-size"] < w["payload-size"]:
            invariants = False
        if not (1 <= r["ponymaxthreads"] <= 64):
            invariants = False
        if r.get("ponypinasio") and not r.get("ponypin"):
            invariants = False
    dp = o.DEFAULT_PROFILE
    check("all payload sizes appear",
          payloads == set(dp["payload_sizes"]))
    check("all drawn batch sizes appear (burst clamp may add others)",
          set(dp["batch_sizes"]).issubset(batches))
    check("all client counts appear (some clamped by burst cap)",
          set(dp["clients"]).issubset(clients))
    check("all read buffer sizes appear",
          set(dp["read_buffer_sizes"]).issubset(rbufs))
    check("all max-datagrams-per-turn values appear",
          max_dgs == set(dp["max_datagrams_per_turn"]))
    check("ponynoscale is drawn sometimes and not always",
          0 < noscale < 500)
    check("ponypin and ponypinasio both appear", pin > 0 and pinasio > 0)
    check("ponynoblock is drawn sometimes and not always",
          0 < noblock < 500)
    check("per-seed invariants hold across 500 seeds", invariants)


def test_burst_clamp():
    # With many clients and large payloads, the batch is clamped.
    clamped = 0
    for seed in range(500):
        w = o.resolve_config(seed, 64)["workload"]
        c, b, p = w["clients"], w["batch-size"], max(1, w["payload-size"])
        byte_ok = (c * b * p) <= o.MAX_BURST_BYTES
        dgram_ok = (c * b) <= o.MAX_BURST_DATAGRAMS
        if not (byte_ok and dgram_ok):
            clamped = -1
            break
        if b < max(o.DEFAULT_PROFILE["batch_sizes"]):
            clamped += 1
    check("burst clamp: every seed is under the ceiling", clamped >= 0)
    check("burst clamp: some seeds had their batch reduced", clamped > 0)


def test_read_buffer_at_least_payload():
    # The read buffer is bumped to at least the payload size. Check that for a
    # large payload, this guarantee holds.
    ok = True
    for seed in range(500):
        w = o.resolve_config(seed, 8)["workload"]
        if w["read-buffer-size"] < w["payload-size"]:
            ok = False
    check("read buffer >= payload size for all seeds", ok)


def test_host_dependent_clamping():
    runtime_stable = True
    differed = 0
    bounded_low = True
    exceeded_low = False
    for seed in range(200):
        a = o.resolve_config(seed, 4)
        b = o.resolve_config(seed, 64)
        # Runtime flags other than ponymaxthreads are drawn the same
        # regardless of core count (the RNG sequence is identical).
        ra = dict(a["runtime"])
        rb = dict(b["runtime"])
        ta = ra.pop("ponymaxthreads", None)
        tb = rb.pop("ponymaxthreads", None)
        if ra != rb:
            runtime_stable = False
        if ta != tb:
            differed += 1
        if not (1 <= ta <= 4):
            bounded_low = False
        if tb > 4:
            exceeded_low = True
    check("runtime flags (except threads) are stable across core counts",
          runtime_stable)
    check("ponymaxthreads is bounded by the (smaller) core count", bounded_low)
    check("ponymaxthreads genuinely differs across core counts",
          differed > 0 and exceeded_low)


def test_resolve_seeds():
    from types import SimpleNamespace

    def ns(**kw):
        base = dict(master_seed=None, replay=None, count=None, seeds=None, start=0)
        base.update(kw)
        return SimpleNamespace(**base)

    check("resolve_seeds: --master-seed runs just that seed",
          o.resolve_seeds(ns(master_seed=5)) == [5])
    check("resolve_seeds: --replay runs just that seed",
          o.resolve_seeds(ns(replay=9)) == [9])
    check("resolve_seeds: --count runs a range from --start",
          o.resolve_seeds(ns(count=3, start=10)) == [10, 11, 12])
    check("resolve_seeds: --seeds parses a CSV list",
          o.resolve_seeds(ns(seeds="1,2,3")) == [1, 2, 3])
    check("resolve_seeds: no selector runs the single --start seed",
          o.resolve_seeds(ns(start=7)) == [7])
    import contextlib
    import io
    threw = False
    try:
        with contextlib.redirect_stderr(io.StringIO()):
            o.resolve_seeds(ns(seeds="1,x,3"))
    except SystemExit:
        threw = True
    check("resolve_seeds: a non-integer in --seeds dies", threw)


def test_validate_args():
    import contextlib
    import io

    def dies(**kw):
        base = dict(no_progress_seconds=300, timeout_seconds=3000,
                    mem_limit_mb=4096)
        base.update(kw)
        threw = False
        try:
            with contextlib.redirect_stderr(io.StringIO()):
                o.validate_args(**base)
        except SystemExit:
            threw = True
        return threw

    check("validate_args: valid defaults pass", not dies())
    check("validate_args: --no-progress-seconds at the 5s heartbeat dies",
          dies(no_progress_seconds=5))
    check("validate_args: --timeout-seconds <= --no-progress-seconds dies",
          dies(timeout_seconds=300, no_progress_seconds=300))
    check("validate_args: negative --mem-limit-mb dies", dies(mem_limit_mb=-1))
    check("validate_args: --mem-limit-mb 0 is allowed (explicit disable)",
          not dies(mem_limit_mb=0))


def test_build_argv():
    config = {
        "master_seed": 0,
        "workload": {"datagrams": 100, "payload-size": 64},
        "runtime": {"ponynoscale": True, "ponymaxthreads": 3},
    }
    argv = o.build_argv("/bin/udp_flood", config)
    check("build_argv: binary first", argv[0] == "/bin/udp_flood")
    check("build_argv: workload as --key value",
          "--datagrams" in argv and argv[argv.index("--datagrams") + 1] == "100")
    check("build_argv: True flag is bare", "--ponynoscale" in argv
          and "True" not in argv)
    check("build_argv: valued runtime flag",
          argv[argv.index("--ponymaxthreads") + 1] == "3")


def test_parse_result():
    out = ("RESULT clients=4 client_sent=400 server_received=400 "
           "server_corrupted=0 client_send_errors=0 bind_failed=0\nPASS")
    parsed = o.parse_result(out)
    check("parse_result: reads the tally",
          parsed == {"clients": 4, "client_sent": 400, "server_received": 400,
                     "server_corrupted": 0, "client_send_errors": 0,
                     "bind_failed": 0})
    check("parse_result: empty on garbage", o.parse_result("nothing here") == {})


def test_lldb_argv():
    argv = o.lldb_argv("/usr/bin/lldb", ["/bin/udp_flood", "--datagrams", "100"])
    joined = " ".join(argv)
    check("lldb_argv: --batch present", "--batch" in argv)
    check("lldb_argv: captures a backtrace on crash", "bt all" in argv)
    sep = argv.index("--")
    check("lldb_argv: engine argv follows --",
          argv[sep + 1:] == ["/bin/udp_flood", "--datagrams", "100"])
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
    check("watchdog: no_progress when done stalls past the window",
          o._watchdog_kill_reason(now=1000, start=0, last_progress=699,
                                  timeout=3000, no_progress_seconds=300)
          == "no_progress")
    check("watchdog: backstop when still advancing past the cap",
          o._watchdog_kill_reason(now=4000, start=0, last_progress=3900,
                                  timeout=3000, no_progress_seconds=300)
          == "backstop")
    check("watchdog: None when progressing inside the cap",
          o._watchdog_kill_reason(now=100, start=0, last_progress=100,
                                  timeout=3000, no_progress_seconds=300) is None)
    check("watchdog: a stall past the cap is a hang, not a backstop",
          o._watchdog_kill_reason(now=4000, start=0, last_progress=3000,
                                  timeout=3000, no_progress_seconds=300)
          == "no_progress")
    check("watchdog: a gap exactly at the window does not fire",
          o._watchdog_kill_reason(now=300, start=0, last_progress=0,
                                  timeout=3000, no_progress_seconds=300) is None)
    check("watchdog: one past the window fires no_progress",
          o._watchdog_kill_reason(now=301, start=0, last_progress=0,
                                  timeout=3000, no_progress_seconds=300)
          == "no_progress")
    check("watchdog: elapsed exactly at the backstop does not fire",
          o._watchdog_kill_reason(now=3000, start=0, last_progress=3000,
                                  timeout=3000, no_progress_seconds=300) is None)
    check("watchdog: one past the backstop fires backstop",
          o._watchdog_kill_reason(now=3001, start=0, last_progress=3001,
                                  timeout=3000, no_progress_seconds=300)
          == "backstop")


def test_parse_reported():
    check("parse_reported: reads the count from a heartbeat",
          o._parse_reported(b"HEARTBEAT reported=3 of 4\n") == 3)
    check("parse_reported: None on the RESULT line",
          o._parse_reported(b"RESULT clients=4 client_sent=400\n") is None)
    check("parse_reported: None on a bare reported= without the HEARTBEAT prefix",
          o._parse_reported(b"reported=5 but not a heartbeat\n") is None)


def test_is_progress():
    check("is_progress: the first count seen is progress",
          o._is_progress(0, -1))
    check("is_progress: a rising count is progress", o._is_progress(6, 5))
    check("is_progress: a frozen count is not progress", not o._is_progress(5, 5))
    check("is_progress: a lower count is not progress", not o._is_progress(3, 5))
    check("is_progress: None is not progress", not o._is_progress(None, 5))


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
    real_kill = o._kill_process_tree
    killed = []
    o._kill_process_tree = lambda p: killed.append(p)
    try:
        proc = _FakeProc(
            [b"HEARTBEAT reported=1 of 4\n", b"RESULT clients=4\n"],
            [b"debug output\n"], alive_polls=0, returncode=0)
        reason, rc, out, err = o._watch_for_progress(
            proc, 3000, 300, poll=lambda: 0.0, sleep=lambda _s: None)
        check("watch: a clean run has no kill reason", reason is None)
        check("watch: a clean run's exit code passes through", rc == 0)
        check("watch: a clean run drains stdout", "RESULT" in out)
        check("watch: a clean run drains stderr", "debug" in err)
        check("watch: a clean run is not killed", not killed)

        clock = [0.0]

        def advance(_s):
            clock[0] += 1000.0

        frozen = [b"HEARTBEAT reported=2 of 4\n"] * 4
        proc2 = _FakeProc(frozen, [], alive_polls=1000, returncode=0)
        reason2, rc2, _o, _e = o._watch_for_progress(
            proc2, 3000, 300, poll=lambda: clock[0], sleep=advance)
        check("watch: heartbeats with a frozen reported count is a hang",
              reason2 == "no_progress")
        check("watch: a hung run is killed", len(killed) == 1)
        check("watch: a hung run has no returncode", rc2 is None)

        clock3 = [0.0]

        def small_advance(_s):
            clock3[0] += 100.0

        proc3 = _FakeProc([b"HEARTBEAT reported=1 of 4\n"], [], alive_polls=2,
                          returncode=0)
        reason3, rc3, _o3, _e3 = o._watch_for_progress(
            proc3, 3000, 300, poll=lambda: clock3[0], sleep=small_advance)
        check("watch: a run advancing inside the window completes",
              (reason3 is None) and (rc3 == 0))
        check("watch: a spared run is not killed", len(killed) == 1)
    finally:
        o._kill_process_tree = real_kill


_MIN_CONFIG = {"master_seed": 0, "workload": {"datagrams": 1}, "runtime": {}}


def _fake_capture(kill_reason, returncode, stdout, stderr=""):
    return lambda *a, **k: (kill_reason, returncode, stdout, stderr)


def test_run_once():
    real = o._capture
    try:
        o._capture = _fake_capture(None, 0, "ok")
        r = o.run_once("/bin/x", _MIN_CONFIG, 3000, None, 300)
        check("run_once: clean exit 0 -> pass",
              r.outcome == "pass" and r.returncode == 0 and r.signal is None)
        o._capture = _fake_capture(None, 1, "", "mismatch")
        r = o.run_once("/bin/x", _MIN_CONFIG, 3000, None, 300)
        check("run_once: clean exit 1 -> fail",
              r.outcome == "fail" and r.returncode == 1)
        o._capture = _fake_capture(None, -11, "", "")
        r = o.run_once("/bin/x", _MIN_CONFIG, 3000, None, 300)
        check("run_once: a negative code -> fail with signal number 11",
              r.outcome == "fail" and r.signal == 11)
        o._capture = _fake_capture("no_progress", None, "", "")
        r = o.run_once("/bin/x", _MIN_CONFIG, 3000, None, 300)
        check("run_once: a no-progress kill -> hang",
              r.outcome == "hang" and o._is_failure(r.outcome))
        o._capture = _fake_capture("backstop", None, "", "")
        r = o.run_once("/bin/x", _MIN_CONFIG, 3000, None, 300)
        check("run_once: a backstop kill -> incomplete (NOT a failure)",
              r.outcome == "incomplete" and not o._is_failure(r.outcome))
    finally:
        o._capture = real


def test_run_under_lldb():
    real = o._capture
    try:
        o._capture = _fake_capture(None, 0, "Process 7 exited with status = 0 (0x0)")
        r = o.run_under_lldb("/bin/x", "lldb", _MIN_CONFIG, 3000, None, 300)
        check("run_under_lldb: clean exit 0 -> pass",
              r.outcome == "pass" and r.returncode == 0)
        o._capture = _fake_capture(None, 0, "Process 7 exited with status = 1 (0x1)")
        r = o.run_under_lldb("/bin/x", "lldb", _MIN_CONFIG, 3000, None, 300)
        check("run_under_lldb: exit 1 -> fail",
              r.outcome == "fail" and r.returncode == 1)
        o._capture = _fake_capture(
            None, 1, "stopped\n* stop reason = signal SIGSEGV\nbt all ...")
        r = o.run_under_lldb("/bin/x", "lldb", _MIN_CONFIG, 3000, None, 300)
        check("run_under_lldb: a crash -> fail with the signal name",
              r.outcome == "fail" and r.signal == "SIGSEGV")
        o._capture = _fake_capture(None, 1, "garbage with no markers")
        r = o.run_under_lldb("/bin/x", "lldb", _MIN_CONFIG, 3000, None, 300)
        check("run_under_lldb: no markers -> fail",
              r.outcome == "fail" and r.signal == "crash")
        o._capture = _fake_capture("no_progress", None, "partial")
        r = o.run_under_lldb("/bin/x", "lldb", _MIN_CONFIG, 3000, None, 300)
        check("run_under_lldb: a no-progress kill -> hang",
              r.outcome == "hang" and o._is_failure(r.outcome))
        o._capture = _fake_capture("backstop", None, "partial")
        r = o.run_under_lldb("/bin/x", "lldb", _MIN_CONFIG, 3000, None, 300)
        check("run_under_lldb: a backstop kill -> incomplete (NOT a failure)",
              r.outcome == "incomplete" and not o._is_failure(r.outcome))
        o._capture = _fake_capture(
            None, 1,
            "stop reason = signal SIGABRT\n"
            "  Process 1 exited with status = 0")
        r = o.run_under_lldb("/bin/x", "lldb", _MIN_CONFIG, 3000, None, 300)
        check("run_under_lldb: an embedded status in a crash dump is not a pass",
              r.outcome == "fail" and r.signal == "SIGABRT")
    finally:
        o._capture = real


def test_rlimit_as_supported():
    check("rlimit: on for linux with resource",
          o._rlimit_as_supported("linux", True))
    check("rlimit: off for macOS", not o._rlimit_as_supported("darwin", True))
    check("rlimit: off for windows", not o._rlimit_as_supported("win32", True))
    check("rlimit: off when the resource module is absent",
          not o._rlimit_as_supported("linux", False))


def main():
    for fn in (test_resolve_config_deterministic, test_clamp_run,
               test_resolve_config_coverage, test_burst_clamp,
               test_read_buffer_at_least_payload,
               test_host_dependent_clamping,
               test_resolve_seeds, test_validate_args,
               test_build_argv, test_parse_result, test_lldb_argv,
               test_lldb_exit_code, test_watchdog_kill_reason,
               test_parse_reported, test_is_progress, test_classify_outcome,
               test_is_failure, test_watch_for_progress, test_run_once,
               test_run_under_lldb, test_rlimit_as_supported):
        fn()
    if FAILURES:
        print("\n%d failure(s): %s" % (len(FAILURES), ", ".join(FAILURES)))
        return 1
    print("\nall orchestrate_udp_test checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
