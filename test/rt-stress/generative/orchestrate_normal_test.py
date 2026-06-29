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
import stress_common as common

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
    # the systematic golden -- normal draws bucketed chains/ttl, a payload whose string
    # size is limited by the message count, and ponynoblock as a swarm knob, and
    # carries no systematic seed.
    # (This intentionally differs from the pre-magnitude normal golden -- the new
    # draw remaps every historical seed.)
    expected = {
        "master_seed": 0,
        "runtime": {
            "ponycdinterval": 10,
            "ponygcinitial": 1024,
            "ponymaxthreads": 5,
        },
        "workload": {
            "chains": 20891,
            "payload": "u64",
            "payload-mode": "fresh",
            "payload-size": 4096,
            "pingers": 64,
            "seed": 2686188150644990173,
            "ttl": 9876,
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

    # pingers=1 is drawn in normal mode -- it exercises the ORCA self-send path
    # (bounded since PR #5594; see NORMAL_PINGERS).
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
        # chains/ttl now span the magnitude buckets' union (small low .. large high).
        if not (1500 <= work["chains"] <= 34000):
            invariants_hold = False
        if not (1500 <= work["ttl"] <= 34000):
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


def test_resolve_config_magnitude_buckets():
    # The deliberate small->large distribution: chains and ttl each draw a
    # small/medium/large bucket, and over many seeds BOTH knobs must reach all three.
    b = common.NORMAL_SIZE_BUCKETS
    chains_buckets = {"small": 0, "medium": 0, "large": 0}
    ttl_buckets = {"small": 0, "medium": 0, "large": 0}
    for master in range(0, 300):
        w = normal.resolve_config(master, THREADS)["workload"]
        for val, seen in ((w["chains"], chains_buckets), (w["ttl"], ttl_buckets)):
            if val <= b["small"][1]:
                seen["small"] += 1
            elif val <= b["medium"][1]:
                seen["medium"] += 1
            else:
                seen["large"] += 1
    check("chains reaches all three magnitude buckets",
          all(c > 0 for c in chains_buckets.values()))
    check("ttl reaches all three magnitude buckets",
          all(c > 0 for c in ttl_buckets.values()))


def test_resolve_config_draws_single_pinger():
    # pingers=1 must be drawn in normal mode: it exercises the ORCA self-send path
    # (bounded since PR #5594), so the soak keeps a standing stress on that fix.
    one_seen = False
    for master in range(0, 500):
        if normal.resolve_config(master, THREADS)["workload"]["pingers"] == 1:
            one_seen = True
    check("normal mode draws pingers=1", one_seen)


def test_resolve_config_fresh_string_size_fits_run():
    # A fresh-string config's size must fit its run: above the medium table's cap a
    # fresh string may only be a small size; above the large table's cap, never 4096.
    caps = {name: cap for name, _t, cap in common.STRING_SIZE_TABLES}
    small = set(common.STRING_SIZE_TABLES[0][1])
    small_medium = small | set(common.STRING_SIZE_TABLES[1][1])
    holds = True
    fresh_seen = False
    for master in range(0, 400):
        w = normal.resolve_config(master, THREADS)["workload"]
        if not (w["payload"] == "string" and w["payload-mode"] == "fresh"):
            continue
        fresh_seen = True
        msgs = w["chains"] * (w["ttl"] + 1)
        if msgs > caps["medium"] and w["payload-size"] not in small:
            holds = False
        elif msgs > caps["large"] and w["payload-size"] not in small_medium:
            holds = False
    check("fresh-string size fits the run's message count", holds)
    check("fresh-string configs were actually drawn to check", fresh_seen)


def main():
    test_resolve_config_golden()
    test_resolve_config_never_sets_systematic_seed()
    test_ponynoblock_is_a_swarm_knob()
    test_resolve_config_deterministic_and_bounded()
    test_resolve_config_swarm_omission()
    test_resolve_config_magnitude_buckets()
    test_resolve_config_draws_single_pinger()
    test_resolve_config_fresh_string_size_fits_run()
    if FAILURES:
        print("%d failure(s): %s" % (len(FAILURES), ", ".join(FAILURES)))
        sys.exit(1)
    print("all orchestrate_normal_test checks passed")


if __name__ == "__main__":
    main()
