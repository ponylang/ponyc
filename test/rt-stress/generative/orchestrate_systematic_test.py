#!/usr/bin/env python3
"""Unit tests for orchestrate_systematic.py's resolve_config (the systematic-mode
config draw). Self-contained (no pytest): `python3 ..._test.py`, exits 0/1.

The golden below is the SAME value the pre-refactor orchestrate.py pinned: it
guards that the systematic seed->config mapping is byte-identical across the
split into stress_common.py, so historical --replay for any seed still
reconstructs the same run.
"""
import sys

import orchestrate_systematic as systematic

FAILURES = []

# A fixed physical-core count so resolve_config is deterministic in tests (the
# real orchestrator probes it from the runtime).
THREADS = 8


def check(name, condition):
    if condition:
        print("ok   - " + name)
    else:
        print("FAIL - " + name)
        FAILURES.append(name)


def test_resolve_config_golden():
    # Pin resolve_config(0, 8)'s EXACT output: the seed-stability guard. Any change
    # to the draw sequence (a narrowed range, an inserted/reordered draw, a moved
    # payload-mode) changes this and fails here -- which also flags that historical
    # --replay for this seed has been broken.
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
          systematic.resolve_config(0, THREADS) == expected)


def test_resolve_config_deterministic_and_bounded():
    check("resolve_config is deterministic",
          systematic.resolve_config(7, THREADS)
          == systematic.resolve_config(7, THREADS))

    invariants_hold = True
    maxthreads_seen = set()
    for master in range(0, 300):
        config = systematic.resolve_config(master, THREADS)
        runtime = config["runtime"]
        # ponynoblock is ALWAYS forced on in systematic (the determinism oracle
        # needs the cycle detector off); the systematic seed is always set >= 1.
        if runtime.get("ponynoblock") is not True:
            invariants_hold = False
        if runtime["ponysystematictestingseed"] < 1:
            invariants_hold = False
        mt = runtime.get("ponymaxthreads")
        if (mt is None) or not (1 <= mt <= THREADS):
            invariants_hold = False
        else:
            maxthreads_seen.add(mt)
    check("systematic invariants hold over 300 seeds", invariants_hold)
    check("ponymaxthreads spans the full [1, THREADS] range",
          maxthreads_seen == set(range(1, THREADS + 1)))


def test_resolve_config_swarm_omission():
    # The four optional knobs must each appear in some runs and be absent in
    # others (ponynoblock and ponymaxthreads are always set, so not omittable).
    omittable = ["ponynoscale", "ponygcinitial", "ponygcfactor", "ponycdinterval"]
    counts = dict.fromkeys(omittable, 0)
    for master in range(0, 300):
        runtime = systematic.resolve_config(master, THREADS)["runtime"]
        for knob in omittable:
            if knob in runtime:
                counts[knob] += 1
    for knob in omittable:
        check("swarm omission: %s sometimes present" % knob, counts[knob] > 0)
        check("swarm omission: %s sometimes absent" % knob, counts[knob] < 300)


def main():
    test_resolve_config_golden()
    test_resolve_config_deterministic_and_bounded()
    test_resolve_config_swarm_omission()
    if FAILURES:
        print("%d failure(s): %s" % (len(FAILURES), ", ".join(FAILURES)))
        sys.exit(1)
    print("all orchestrate_systematic_test checks passed")


if __name__ == "__main__":
    main()
