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
    for master in range(0, 300):
        runtime = {}
        common.draw_swarm_knobs(random.Random(master), runtime)
        if "ponynoblock" in runtime:
            ponynoblock_added = True
        for knob in omittable:
            if knob in runtime:
                counts[knob] += 1
    for knob in omittable:
        check("swarm omission: %s sometimes present" % knob, counts[knob] > 0)
        check("swarm omission: %s sometimes absent" % knob, counts[knob] < 300)
    check("draw_swarm_knobs never adds ponynoblock", not ponynoblock_added)


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
    # A fresh string's available SIZE is limited by the run's message count; forward
    # and u64 are flat-cost and draw any size. The harness's largest run has chains and
    # ttl both at the large bucket's max -- max*(max+1) messages, ~1.16B, past even the
    # small table's budget -- so a fresh string there may draw only small sizes. Derive
    # it from the bucket so it can't go stale if the ranges change.
    _max_dim = common.NORMAL_SIZE_BUCKETS["large"][1]
    big = _max_dim * (_max_dim + 1)
    small_sizes = set(common.STRING_SIZE_TABLES[0][1])   # {1, 8, 64}
    larger_sizes = set(common.ALL_STRING_SIZES) - small_sizes   # {256, 1024, 4096}
    caps = {name: cap for name, _t, cap in common.STRING_SIZE_TABLES}

    big_fresh_larger = big_fresh_small = flat_larger_at_big = False
    for master in range(0, 2000):
        k, s, m = common.draw_payload(random.Random(master), big)
        if k == "string" and m == "fresh":
            big_fresh_larger |= s in larger_sizes
            big_fresh_small |= s in small_sizes
        else:                                 # forward string or u64
            flat_larger_at_big |= s in larger_sizes
    check("draw_payload: a big fresh-string run never draws a non-small size",
          not big_fresh_larger)
    check("draw_payload: a big fresh-string run still draws small sizes",
          big_fresh_small)
    check("draw_payload: forward/u64 draw any size even at the max run",
          flat_larger_at_big)

    # A big run's fresh-string sizes are a strict subset of a small run's: the table
    # only grows as the run shrinks.
    fresh_big, fresh_small = set(), set()
    for master in range(0, 3000):
        k, s, m = common.draw_payload(random.Random(master), big)
        if k == "string" and m == "fresh":
            fresh_big.add(s)
        k, s, m = common.draw_payload(random.Random(master), 1)
        if k == "string" and m == "fresh":
            fresh_small.add(s)
    check("draw_payload: a small run can draw a large fresh-string size",
          larger_sizes & fresh_small)
    check("draw_payload: a big run's fresh sizes are a strict subset of a small run's",
          fresh_big < fresh_small)

    # Cap boundary, per table: at exactly a table's cap a fresh string may still draw
    # that table's sizes; one message above, the table drops out. The small table is
    # the floor -- above its cap the never-deny fallback keeps its sizes -- so it is
    # checked separately below; here we cover medium and large.
    def fresh_sizes_at(msgs):
        seen = set()
        for master in range(0, 3000):
            k, s, m = common.draw_payload(random.Random(master), msgs)
            if k == "string" and m == "fresh":
                seen.add(s)
        return seen
    for name, table_sizes, cap in common.STRING_SIZE_TABLES[1:]:   # medium, large
        want = set(table_sizes)
        at_cap, above_cap = fresh_sizes_at(cap), fresh_sizes_at(cap + 1)
        check("draw_payload: fresh string AT the %s cap can draw %s sizes"
              % (name, name), want <= at_cap)
        check("draw_payload: fresh string ABOVE the %s cap cannot draw %s sizes"
              % (name, name), not (want & above_cap))

    # The small table is the floor: at AND above its cap a fresh string still draws its
    # sizes (the never-deny fallback) and never a larger one.
    small_set = set(common.STRING_SIZE_TABLES[0][1])
    at_small, above_small = fresh_sizes_at(caps["small"]), fresh_sizes_at(caps["small"] + 1)
    check("draw_payload: fresh string AT the small cap can draw small sizes",
          small_set <= at_small)
    check("draw_payload: fresh string ABOVE the small cap still draws small sizes",
          small_set <= above_small)
    check("draw_payload: fresh string ABOVE the small cap draws no larger size",
          not (above_small - small_set))

    # Seed-stability contract: kind and mode are drawn before the (msgs-dependent)
    # size, and the size always costs exactly one rng draw, so draw_payload consumes
    # the SAME rng count for any msgs -- the message count must never remap a
    # downstream draw. Verify the rng state AFTER the call is identical across msgs
    # spanning every cap boundary and the overrun region (msgs > the small cap, where
    # the fallback fires), and that kind/mode never shift with msgs.
    boundary_msgs = [1, caps["large"], caps["large"] + 1, caps["medium"],
                     caps["medium"] + 1, caps["small"], caps["small"] + 1, big]
    rng_stable = kind_mode_stable = True
    for master in range(0, 300):
        states, kinds_modes = [], []
        for mm in boundary_msgs:
            rng = random.Random(master)
            k, _, m = common.draw_payload(rng, mm)
            states.append(rng.getstate())
            kinds_modes.append((k, m))
        if any(st != states[0] for st in states):
            rng_stable = False
        if any(km != kinds_modes[0] for km in kinds_modes):
            kind_mode_stable = False
    check("draw_payload: rng consumption is fixed across all msgs (overrun included)",
          rng_stable)
    check("draw_payload: message count changes only the size, never kind/mode",
          kind_mode_stable)


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
    test_build_argv()
    test_run_command()
    test_lldb_argv()
    test_lldb_exit_code()
    test_run_once()
    test_run_under_lldb()
    test_rlimit_as_supported()
    test_bundle_for()
    test_resolve_seeds()
    test_parse_result()
    if FAILURES:
        print("%d failure(s): %s" % (len(FAILURES), ", ".join(FAILURES)))
        sys.exit(1)
    print("all stress_common_test checks passed")


if __name__ == "__main__":
    main()
