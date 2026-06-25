#!/usr/bin/env python3
"""Unit tests for the pure pieces of orchestrate.py.

Self-contained (no pytest): runnable as
`python3 test/rt-stress/generative/orchestrate_test.py`, exits 0 on pass / 1 on
failure, prints a one-line summary. Picked up automatically by lint-python.yml's
`*_test.py` discovery once that workflow covers test/rt-stress/.
"""
import sys

import orchestrate

FAILURES = []

U64_MAX = (1 << 64) - 1
# A fixed physical-core count so resolve_config is deterministic in tests (the
# real orchestrator probes it from the runtime).
THREADS = 8


def check(name, condition):
    if condition:
        print("ok   - " + name)
    else:
        print("FAIL - " + name)
        FAILURES.append(name)


def test_derive_seeds():
    check("derive_seeds is deterministic",
          orchestrate.derive_seeds(42) == orchestrate.derive_seeds(42))

    # Both seeds stay in U64 and >= 1 across a spread of master seeds. The
    # systematic seed >= 1 is load-bearing -- the runtime rejects 0.
    all_in_range = True
    program_seen = set()
    for master in range(0, 300):
        program, systematic = orchestrate.derive_seeds(master)
        program_seen.add(program)
        if not (1 <= program <= U64_MAX and 1 <= systematic <= U64_MAX):
            all_in_range = False
    check("derive_seeds keeps both seeds in [1, U64_MAX]", all_in_range)

    # The two derived seeds are independent (different labels), and the program
    # seed varies with the master seed rather than being constant.
    program0, systematic0 = orchestrate.derive_seeds(0)
    check("derive_seeds: program != systematic", program0 != systematic0)
    check("derive_seeds: program seed varies with master",
          len(program_seen) > 250)


def test_resolve_config_deterministic_and_bounded():
    check("resolve_config is deterministic",
          orchestrate.resolve_config(7, THREADS)
          == orchestrate.resolve_config(7, THREADS))

    pingers_allowed = {1, 2, 4, 8, 16, 32, 64}
    size_allowed = {1, 8, 64, 256, 1024, 4096}
    invariants_hold = True
    pingers_seen = set()
    size_seen = set()
    chains_seen = []
    ttls_seen = []
    payload_seen = set()
    mode_seen = set()
    maxthreads_ok = True
    maxthreads_seen = set()
    for master in range(0, 300):
        config = orchestrate.resolve_config(master, THREADS)
        work = config["workload"]
        runtime = config["runtime"]
        if runtime.get("ponynoscale") is not True:
            invariants_hold = False
        if runtime["ponysystematictestingseed"] < 1:
            invariants_hold = False
        if work["pingers"] not in pingers_allowed:
            invariants_hold = False
        if not (1 <= work["chains"] <= 400):
            invariants_hold = False
        if not (0 <= work["ttl"] <= 48):
            invariants_hold = False
        if work["payload"] not in ("string", "u64"):
            invariants_hold = False
        if work["payload-size"] not in size_allowed:
            invariants_hold = False
        mt = runtime.get("ponymaxthreads")
        if (mt is None) or not (1 <= mt <= THREADS):
            maxthreads_ok = False
        else:
            maxthreads_seen.add(mt)
        pingers_seen.add(work["pingers"])
        payload_seen.add(work["payload"])
        mode_seen.add(work["payload-mode"])
        size_seen.add(work["payload-size"])
        chains_seen.append(work["chains"])
        ttls_seen.append(work["ttl"])
    check("resolve_config holds all invariants over 300 seeds", invariants_hold)
    # Edge/coverage, not just membership: the choice knobs must produce their
    # FULL declared set (catches a dropped choice), the randint ranges must span
    # widely (catches a narrowing), and ponymaxthreads must always be set and
    # cover the full [1, max_threads] range (the probed-core-count draw).
    check("pingers covers the full declared set", pingers_seen == pingers_allowed)
    check("payload covers both kinds {string, u64}",
          payload_seen == {"string", "u64"})
    check("payload-mode covers {forward, fresh}",
          mode_seen == {"forward", "fresh"})
    check("payload-size covers the full declared set", size_seen == size_allowed)
    check("ttl reaches both declared edges 0 and 48",
          (min(ttls_seen) == 0) and (max(ttls_seen) == 48))
    check("chains spans most of [1,400]",
          (max(chains_seen) - min(chains_seen)) >= 380)
    check("ponymaxthreads always set and in [1, THREADS]", maxthreads_ok)
    check("ponymaxthreads spans the full [1, THREADS] range",
          maxthreads_seen == set(range(1, THREADS + 1)))


def test_resolve_config_swarm_omission():
    # Swarm testing's mechanism is omission: EACH optional knob must appear in
    # some runs and be absent in others, not pinned one way. Check all four --
    # they share identical `if rng.random() < 0.5` logic, so testing only the
    # first would let a regression in any of the others pass silently.
    # (ponymaxthreads is always set now, so it isn't an omittable knob.)
    omittable = ["ponynoblock", "ponygcinitial", "ponygcfactor",
                 "ponycdinterval"]
    counts = dict.fromkeys(omittable, 0)
    for master in range(0, 300):
        runtime = orchestrate.resolve_config(master, THREADS)["runtime"]
        for knob in omittable:
            if knob in runtime:
                counts[knob] += 1
    for knob in omittable:
        check("swarm omission: %s sometimes present" % knob, counts[knob] > 0)
        check("swarm omission: %s sometimes absent" % knob, counts[knob] < 300)


def test_build_argv():
    config = orchestrate.resolve_config(1, THREADS)
    argv = orchestrate.build_argv("/bin/generative", config)

    check("build_argv: binary is first", argv[0] == "/bin/generative")
    check("build_argv: includes --ponynoscale bare", "--ponynoscale" in argv)
    # A bare flag must not leak its Python True as a literal token.
    check("build_argv: no literal True token", "True" not in argv)

    # Guard the index lookups so a regression that drops a flag yields a clean
    # FAIL line rather than an uncaught ValueError traceback.
    program_seed = str(config["workload"]["seed"])
    check("build_argv: --seed present", "--seed" in argv)
    if "--seed" in argv:
        check("build_argv: --seed carries the program seed",
              argv[argv.index("--seed") + 1] == program_seed)

    check("build_argv: --ponysystematictestingseed present",
          "--ponysystematictestingseed" in argv)
    if "--ponysystematictestingseed" in argv:
        check("build_argv: systematic seed value present",
              argv[argv.index("--ponysystematictestingseed") + 1] ==
              str(config["runtime"]["ponysystematictestingseed"]))


def test_run_command():
    config = orchestrate.resolve_config(1, THREADS)
    engine = orchestrate.build_argv("/bin/generative", config)
    full = orchestrate.run_command("/bin/generative", config)
    prefix = orchestrate.aslr_prefix()

    check("run_command: engine argv is the suffix",
          full[len(full) - len(engine):] == engine)
    check("run_command: length is prefix + engine",
          len(full) == len(prefix) + len(engine))
    # The ASLR prefix is either absent or a `setarch ... -R` invocation.
    check("aslr_prefix: empty or setarch -R",
          (prefix == []) or ((prefix[0] == "setarch") and (prefix[-1] == "-R")))


def test_derive_seeds_zero_floor():
    # The `or 1` floor on both derived seeds is load-bearing: the runtime rejects
    # systematic seed 0, and Rand(0, 0) is the degenerate all-zero xoroshiro
    # state. No real SHA-256 prefix is zero in the seed ranges used, so force a
    # zero digest to exercise the floor directly (otherwise it is never hit).
    class _ZeroDigest:
        def digest(self):
            return b"\x00" * 32

    def _zero_sha(*args, **kwargs):
        return _ZeroDigest()

    real = orchestrate.hashlib.sha256
    try:
        orchestrate.hashlib.sha256 = _zero_sha
        program, systematic = orchestrate.derive_seeds(123)
        check("derive_seeds floors a zero program digest to 1", program == 1)
        check("derive_seeds floors a zero systematic digest to 1",
              systematic == 1)
    finally:
        orchestrate.hashlib.sha256 = real


def test_build_argv_numeric_knob():
    # Numeric runtime-knob values must render as their string form (e.g. a float
    # gcfactor), not be dropped or mangled.
    config = {
        "master_seed": 0,
        "workload": {"seed": 5, "pingers": 8, "chains": 8, "ttl": 16,
                     "payload": "string", "payload-size": 64},
        "runtime": {"ponynoscale": True, "ponysystematictestingseed": 7,
                    "ponygcfactor": 1.05, "ponymaxthreads": 2},
    }
    argv = orchestrate.build_argv("/bin/generative", config)
    check("build_argv renders a float knob value",
          ("--ponygcfactor" in argv)
          and (argv[argv.index("--ponygcfactor") + 1] == "1.05"))
    check("build_argv renders an int knob value",
          ("--ponymaxthreads" in argv)
          and (argv[argv.index("--ponymaxthreads") + 1] == "2"))


def test_resolve_config_golden():
    # Pin resolve_config(0)'s EXACT output: the seed-stability guard. Any change
    # to the draw sequence (a narrowed range, an inserted/reordered draw, a
    # dropped choice, a removed seed floor) changes this and fails here -- which
    # also flags that historical --replay for this seed has been broken.
    expected = {
        "master_seed": 0,
        "runtime": {
            "ponygcfactor": 2.0,
            "ponymaxthreads": 4,
            "ponynoblock": True,
            "ponynoscale": True,
            "ponysystematictestingseed": 6974751086576699578,
        },
        "workload": {
            "chains": 198,
            "payload": "u64",
            "payload-mode": "fresh",
            "payload-size": 1,
            "pingers": 64,
            "seed": 2686188150644990173,
            "ttl": 48,
        },
    }
    check("resolve_config(0, 8) matches the pinned golden config",
          orchestrate.resolve_config(0, THREADS) == expected)


def test_parse_result():
    line = "RECEIVED=136 SENT=136 EXPECTED=136 ORDER_SIG=13652540024091563273"
    parsed = orchestrate.parse_result(line)
    check("parse_result: received", parsed.get("received") == "136")
    check("parse_result: sent", parsed.get("sent") == "136")
    check("parse_result: expected", parsed.get("expected") == "136")
    check("parse_result: order_sig",
          parsed.get("order_sig") == "13652540024091563273")
    check("parse_result: empty on garbage",
          orchestrate.parse_result("no result here") == {})


def main():
    test_derive_seeds()
    test_derive_seeds_zero_floor()
    test_resolve_config_deterministic_and_bounded()
    test_resolve_config_golden()
    test_resolve_config_swarm_omission()
    test_build_argv()
    test_build_argv_numeric_knob()
    test_run_command()
    test_parse_result()
    if FAILURES:
        print("%d failure(s): %s" % (len(FAILURES), ", ".join(FAILURES)))
        sys.exit(1)
    print("all orchestrate_test checks passed")


if __name__ == "__main__":
    main()
