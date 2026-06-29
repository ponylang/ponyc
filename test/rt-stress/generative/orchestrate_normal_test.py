#!/usr/bin/env python3
"""Unit tests for orchestrate_normal.py's resolve_config (the normal-mode config
draw). Self-contained (no pytest): `python3 ..._test.py`, exits 0/1.

Normal mode is non-reproducible at runtime, but a seed still maps to a stable
*config* -- the golden below pins that draw order. The two things that distinguish
normal from systematic are asserted explicitly: no systematic seed is ever set,
and ponynoblock is a swarm knob (drawn, not forced). Normal mode draws one of two
workload kinds (`mesh` or `cyclic`); the per-kind shape and the memory ceiling on
the cyclic workload are checked here too.
"""
import sys

import orchestrate_normal as normal
import stress_common as common

FAILURES = []

THREADS = 8

PINGERS_ALLOWED = {1, 2, 4, 8, 16, 32, 64}
SIZE_ALLOWED = {1, 8, 64, 256, 1024, 4096}
# Hardcoded calibrated ceiling on generations*group (leaked actors in a CD-off
# run), NOT derived from the CYCLIC_* constants -- bumping a bucket or adding a
# bigger group trips this. Real max today is 8000*16 = 128000 (see CYCLIC_* docs).
CYCLIC_WORKER_CEILING = 130000


def check(name, condition):
    if condition:
        print("ok   - " + name)
    else:
        print("FAIL - " + name)
        FAILURES.append(name)


def test_resolve_config_golden():
    # Pin resolve_config(0, 8): the normal-mode seed-stability guard. Distinct from
    # the systematic golden -- normal draws the workload kind first, then bucketed
    # chains/ttl, a payload whose string size is limited by the message count, and
    # ponynoblock as a swarm knob, and carries no systematic seed.
    # (This intentionally differs from the pre-cyclic normal golden -- drawing the
    # workload kind first remaps every historical seed.)
    expected = {
        "master_seed": 0,
        "runtime": {
            "ponygcinitial": 10,
            "ponymaxthreads": 5,
            "ponynoscale": True,
        },
        "workload": {
            "chains": 30981,
            "payload": "u64",
            "payload-mode": "fresh",
            "payload-size": 64,
            "pingers": 4,
            "seed": 2686188150644990173,
            "ttl": 26842,
            "workload": "mesh",
        },
    }
    check("resolve_config(0, 8) matches the pinned normal golden config",
          normal.resolve_config(0, THREADS) == expected)


def test_resolve_config_cyclic_golden():
    # Pin a CYCLIC config too (seed 0 rolls mesh, so the mesh golden alone never
    # checks the cyclic emit-block shape: generations/group present, no pingers).
    # Seed 1 rolls cyclic.
    expected = {
        "master_seed": 1,
        "runtime": {
            "ponymaxthreads": 5,
            "ponynoscale": True,
        },
        "workload": {
            "chains": 4,
            "generations": 3517,
            "group": 8,
            "payload": "string",
            "payload-mode": "forward",
            "payload-size": 64,
            "seed": 2570779959355939992,
            "ttl": 8,
            "workload": "cyclic",
        },
    }
    check("resolve_config(1, 8) matches the pinned cyclic golden config",
          normal.resolve_config(1, THREADS) == expected)


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
    # detector is stressed when it is absent), not forced on as in systematic. So it
    # must appear in some runs and be absent in others.
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

    # Invariants over both workload kinds: a mesh config carries `pingers` and no
    # `generations`/`group`; a cyclic config the reverse. chains/ttl/payload are
    # shared but draw from the kind's own ranges.
    cyclic_gen_lo = common.CYCLIC_GENERATION_BUCKETS["small"][0]
    cyclic_gen_hi = common.CYCLIC_GENERATION_BUCKETS["large"][1]
    cyclic_chains_hi = common.CYCLIC_CHAINS_BUCKETS["large"][1]
    cyclic_ttl_hi = common.CYCLIC_TTL_BUCKETS["large"][1]
    invariants_hold = True
    maxthreads_seen = set()
    mode_seen = set()
    worst_workers = 0
    for master in range(0, 300):
        config = normal.resolve_config(master, THREADS)
        work = config["workload"]
        runtime = config["runtime"]
        kind = work["workload"]
        if kind == "mesh":
            if ("generations" in work) or ("group" in work):
                invariants_hold = False
            if work["pingers"] not in PINGERS_ALLOWED:
                invariants_hold = False
            # mesh chains/ttl span the magnitude buckets (small low .. large high).
            if not (1500 <= work["chains"] <= 34000):
                invariants_hold = False
            if not (1500 <= work["ttl"] <= 34000):
                invariants_hold = False
        elif kind == "cyclic":
            if "pingers" in work:
                invariants_hold = False
            if not (cyclic_gen_lo <= work["generations"] <= cyclic_gen_hi):
                invariants_hold = False
            if work["group"] not in common.CYCLIC_GROUPS:
                invariants_hold = False
            if not (1 <= work["chains"] <= cyclic_chains_hi):
                invariants_hold = False
            if not (1 <= work["ttl"] <= cyclic_ttl_hi):
                invariants_hold = False
            worst_workers = max(worst_workers,
                                work["generations"] * work["group"])
        else:
            invariants_hold = False
        if work["payload"] not in ("string", "u64"):
            invariants_hold = False
        if work["payload-size"] not in SIZE_ALLOWED:
            invariants_hold = False
        mode_seen.add(work["payload-mode"])
        mt = runtime.get("ponymaxthreads")
        if (mt is None) or not (1 <= mt <= THREADS):
            invariants_hold = False
        else:
            maxthreads_seen.add(mt)
    check("normal per-kind invariants hold over 300 seeds", invariants_hold)
    check("cyclic worst-case workers within the calibrated memory ceiling",
          worst_workers <= CYCLIC_WORKER_CEILING)
    check("payload-mode covers {forward, fresh}",
          mode_seen == {"forward", "fresh"})
    check("ponymaxthreads spans the full [1, THREADS] range",
          maxthreads_seen == set(range(1, THREADS + 1)))


def test_resolve_config_draws_both_workloads():
    # Swarm coverage: over many seeds both workload kinds must appear, or the
    # cyclic path (or the mesh path) would go entirely untested in the soak.
    kinds = set()
    for master in range(0, 300):
        kinds.add(normal.resolve_config(master, THREADS)["workload"]["workload"])
    check("normal mode draws both mesh and cyclic", kinds == {"mesh", "cyclic"})


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
    # The deliberate small->large distribution. A MESH run draws chains/ttl from the
    # magnitude buckets; a CYCLIC run draws generations from its own buckets (and
    # keeps chains/ttl small). Over many seeds each magnitude knob must reach all
    # three of its buckets.
    nb = common.NORMAL_SIZE_BUCKETS
    gb = common.CYCLIC_GENERATION_BUCKETS
    mesh_chains = {"small": 0, "medium": 0, "large": 0}
    mesh_ttl = {"small": 0, "medium": 0, "large": 0}
    cyclic_gen = {"small": 0, "medium": 0, "large": 0}

    def bucket_of(val, buckets):
        if val <= buckets["small"][1]:
            return "small"
        if val <= buckets["medium"][1]:
            return "medium"
        return "large"

    for master in range(0, 400):
        w = normal.resolve_config(master, THREADS)["workload"]
        if w["workload"] == "mesh":
            mesh_chains[bucket_of(w["chains"], nb)] += 1
            mesh_ttl[bucket_of(w["ttl"], nb)] += 1
        else:
            cyclic_gen[bucket_of(w["generations"], gb)] += 1
    check("mesh chains reaches all three magnitude buckets",
          all(c > 0 for c in mesh_chains.values()))
    check("mesh ttl reaches all three magnitude buckets",
          all(c > 0 for c in mesh_ttl.values()))
    check("cyclic generations reaches all three magnitude buckets",
          all(c > 0 for c in cyclic_gen.values()))


def test_resolve_config_draws_single_pinger():
    # pingers=1 must be drawn for some MESH run: it exercises the ORCA self-send
    # path (bounded since PR #5594), so the soak keeps a standing stress on that fix.
    one_seen = False
    for master in range(0, 500):
        w = normal.resolve_config(master, THREADS)["workload"]
        if (w["workload"] == "mesh") and (w["pingers"] == 1):
            one_seen = True
    check("normal mode draws a mesh run with pingers=1", one_seen)


def test_resolve_config_fresh_string_size_fits_run():
    # A fresh-string config's size must fit its run: above the medium table's cap a
    # fresh string may only be a small size; above the large table's cap, never
    # 4096. The run's message count depends on the kind (cyclic multiplies by
    # generations).
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
        if w["workload"] == "cyclic":
            msgs = w["generations"] * w["chains"] * (w["ttl"] + 1)
        else:
            msgs = w["chains"] * (w["ttl"] + 1)
        if msgs > caps["medium"] and w["payload-size"] not in small:
            holds = False
        elif msgs > caps["large"] and w["payload-size"] not in small_medium:
            holds = False
    check("fresh-string size fits the run's message count", holds)
    check("fresh-string configs were actually drawn to check", fresh_seen)


def main():
    test_resolve_config_golden()
    test_resolve_config_cyclic_golden()
    test_resolve_config_never_sets_systematic_seed()
    test_ponynoblock_is_a_swarm_knob()
    test_resolve_config_deterministic_and_bounded()
    test_resolve_config_draws_both_workloads()
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
