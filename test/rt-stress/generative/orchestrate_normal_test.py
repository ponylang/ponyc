#!/usr/bin/env python3
"""Unit tests for orchestrate_normal.py's resolve_config (the normal-mode config
draw). Self-contained (no pytest): `python3 ..._test.py`, exits 0/1.

Normal mode is non-reproducible at runtime, but a seed still maps to a stable
*config* -- the golden below pins that draw order. The two things that distinguish
normal from systematic are asserted explicitly: no systematic seed is ever set,
and ponynoblock is a swarm knob (drawn, not forced).
"""
import sys

import orchestrate_normal as normal

FAILURES = []

THREADS = 8


def check(name, condition):
    if condition:
        print("ok   - " + name)
    else:
        print("FAIL - " + name)
        FAILURES.append(name)


def test_resolve_config_golden():
    # Pin resolve_config(0, 8): the normal-mode seed-stability guard. Distinct from
    # the systematic golden -- normal draws ponynoblock as an extra swarm knob,
    # which shifts every subsequent draw (hence different ponymaxthreads /
    # payload-mode), and carries no systematic seed.
    expected = {
        "master_seed": 0,
        "runtime": {
            "ponycdinterval": 10,
            "ponygcinitial": 65536,
            "ponymaxthreads": 5,
            "ponynoblock": True,
        },
        "workload": {
            "chains": 198,
            "payload": "u64",
            "payload-mode": "forward",
            "payload-size": 1,
            "pingers": 64,
            "seed": 2686188150644990173,
            "ttl": 48,
        },
    }
    check("resolve_config(0, 8) matches the pinned normal golden config",
          normal.resolve_config(0, THREADS) == expected)


def test_resolve_config_never_sets_systematic_seed():
    # The defining guard for normal mode: a normal runtime REJECTS
    # --ponysystematictestingseed, so resolve_config must never emit one.
    none_set = True
    for master in range(0, 300):
        if "ponysystematictestingseed" in normal.resolve_config(
                master, THREADS)["runtime"]:
            none_set = False
    check("normal resolve_config never sets a systematic seed", none_set)


def test_ponynoblock_is_a_swarm_knob():
    # The other defining difference: ponynoblock is DRAWN here (so the cycle
    # detector is stressed when it is absent -- the coverage ubench used to give),
    # not forced on as in systematic. So it must appear in some runs and be absent
    # in others.
    present = absent = 0
    for master in range(0, 300):
        if normal.resolve_config(master, THREADS)["runtime"].get(
                "ponynoblock") is True:
            present += 1
        else:
            absent += 1
    check("ponynoblock sometimes present (cycle detector off)", present > 0)
    check("ponynoblock sometimes absent (cycle detector on, stressed)", absent > 0)


def test_resolve_config_deterministic_and_bounded():
    check("resolve_config is deterministic",
          normal.resolve_config(7, THREADS)
          == normal.resolve_config(7, THREADS))

    pingers_allowed = {1, 2, 4, 8, 16, 32, 64}
    size_allowed = {1, 8, 64, 256, 1024, 4096}
    invariants_hold = True
    maxthreads_seen = set()
    mode_seen = set()
    for master in range(0, 300):
        config = normal.resolve_config(master, THREADS)
        work = config["workload"]
        runtime = config["runtime"]
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
        mode_seen.add(work["payload-mode"])
        mt = runtime.get("ponymaxthreads")
        if (mt is None) or not (1 <= mt <= THREADS):
            invariants_hold = False
        else:
            maxthreads_seen.add(mt)
    check("normal invariants hold over 300 seeds", invariants_hold)
    check("payload-mode covers {forward, fresh}",
          mode_seen == {"forward", "fresh"})
    check("ponymaxthreads spans the full [1, THREADS] range",
          maxthreads_seen == set(range(1, THREADS + 1)))


def test_resolve_config_swarm_omission():
    omittable = ["ponynoscale", "ponygcinitial", "ponygcfactor", "ponycdinterval"]
    counts = dict.fromkeys(omittable, 0)
    for master in range(0, 300):
        runtime = normal.resolve_config(master, THREADS)["runtime"]
        for knob in omittable:
            if knob in runtime:
                counts[knob] += 1
    for knob in omittable:
        check("swarm omission: %s sometimes present" % knob, counts[knob] > 0)
        check("swarm omission: %s sometimes absent" % knob, counts[knob] < 300)


def main():
    test_resolve_config_golden()
    test_resolve_config_never_sets_systematic_seed()
    test_ponynoblock_is_a_swarm_knob()
    test_resolve_config_deterministic_and_bounded()
    test_resolve_config_swarm_omission()
    if FAILURES:
        print("%d failure(s): %s" % (len(FAILURES), ", ".join(FAILURES)))
        sys.exit(1)
    print("all orchestrate_normal_test checks passed")


if __name__ == "__main__":
    main()
