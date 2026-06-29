#!/usr/bin/env python3
"""Unit tests for orchestrate_normal.py's resolve_config (the normal-mode config
draw). Self-contained (no pytest): `python3 ..._test.py`, exits 0/1.

Normal mode is non-reproducible at runtime, but a seed still maps to a stable
*config* -- the per-kind goldens below pin that draw order. The two things that
distinguish normal from systematic are asserted explicitly: no systematic seed is
ever set, and ponynoblock is a swarm knob (drawn, not forced). Normal mode draws
one of three workload kinds (`mesh`, `cyclic`, or `backpressure`); the per-kind
shape, the cyclic memory ceiling, and the backpressure message ceiling are checked
here too.
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
# Hardcoded calibrated ceiling on producers*messages (total backpressure work),
# NOT derived from the BACKPRESSURE_* constants -- bumping a producer count or a
# message bucket trips this and forces a time re-measure. Real max today is
# 256*400000 = 102_400_000 (see BACKPRESSURE_* docs).
BACKPRESSURE_MESSAGE_CEILING = 110_000_000


def check(name, condition):
    if condition:
        print("ok   - " + name)
    else:
        print("FAIL - " + name)
        FAILURES.append(name)


def test_resolve_config_mesh_golden():
    # Pin a MESH config: the normal-mode seed-stability guard for the mesh emit
    # shape (pingers + chains + ttl, no generations/group/producers). Distinct from
    # the systematic golden -- normal draws the workload kind first (now 3-way), then
    # bucketed chains/ttl (a mesh run's ttl then trimmed to the run-time ceiling), a
    # freely-drawn payload, and ponynoblock as a swarm knob, and carries no
    # systematic seed.
    # (This intentionally differs from the two-kind normal golden -- the 3-way kind
    # roll and the extra backpressure-shape draws remap every historical seed.)
    # Seed 1 rolls mesh.
    expected = {
        "master_seed": 1,
        "runtime": {
            "ponycdinterval": 100,
            "ponygcinitial": 10,
            "ponymaxthreads": 1,
        },
        "workload": {
            "chains": 17440,
            "payload": "u64",
            "payload-mode": "fresh",
            "payload-size": 256,
            "pingers": 32,
            "seed": 2570779959355939992,
            "ttl": 1964,
            "workload": "mesh",
        },
    }
    check("resolve_config(1, 8) matches the pinned mesh golden config",
          normal.resolve_config(1, THREADS) == expected)


def test_resolve_config_cyclic_golden():
    # Pin a CYCLIC config too (the mesh golden alone never checks the cyclic emit
    # shape: generations/group/chains/ttl present, no pingers/producers). Seed 5
    # rolls cyclic.
    expected = {
        "master_seed": 5,
        "runtime": {
            "ponycdinterval": 1000,
            "ponygcfactor": 1.05,
            "ponygcinitial": 20,
            "ponymaxthreads": 8,
            "ponynoblock": True,
            "ponynoscale": True,
        },
        "workload": {
            "chains": 6,
            "generations": 2972,
            "group": 2,
            "payload": "u64",
            "payload-mode": "forward",
            "payload-size": 256,
            "seed": 1674455568713221789,
            "ttl": 6,
            "workload": "cyclic",
        },
    }
    check("resolve_config(5, 8) matches the pinned cyclic golden config",
          normal.resolve_config(5, THREADS) == expected)


def test_resolve_config_backpressure_golden():
    # Pin a BACKPRESSURE config: it has producers/messages/apply-every and NO
    # chains/ttl/pingers/generations/group -- the emit shape that proves the new
    # variant carries only its own fields. Seed 0 rolls backpressure.
    expected = {
        "master_seed": 0,
        "runtime": {
            "ponymaxthreads": 3,
            "ponynoblock": True,
        },
        "workload": {
            "apply-every": 200,
            "messages": 354767,
            "payload": "string",
            "payload-mode": "fresh",
            "payload-size": 1,
            "producers": 64,
            "seed": 2686188150644990173,
            "workload": "backpressure",
        },
    }
    check("resolve_config(0, 8) matches the pinned backpressure golden config",
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

    # Invariants over all three workload kinds: each carries ONLY its own shape
    # fields (mesh: pingers+chains+ttl; cyclic: generations+group+chains+ttl;
    # backpressure: producers+messages+apply-every and NO chains/ttl). Payload is
    # shared. This is the structural check that the per-kind emit never leaks a
    # field from another kind.
    cyclic_gen_lo = common.CYCLIC_GENERATION_BUCKETS["small"][0]
    cyclic_gen_hi = common.CYCLIC_GENERATION_BUCKETS["large"][1]
    cyclic_chains_hi = common.CYCLIC_CHAINS_BUCKETS["large"][1]
    cyclic_ttl_hi = common.CYCLIC_TTL_BUCKETS["large"][1]
    bp_msg_lo = common.BACKPRESSURE_MESSAGE_BUCKETS["small"][0]
    bp_msg_hi = common.BACKPRESSURE_MESSAGE_BUCKETS["large"][1]
    invariants_hold = True
    maxthreads_seen = set()
    mode_seen = set()
    for master in range(0, 300):
        config = normal.resolve_config(master, THREADS)
        work = config["workload"]
        runtime = config["runtime"]
        kind = work["workload"]
        if kind == "mesh":
            if any(k in work for k in
                   ("generations", "group", "producers", "messages",
                    "apply-every")):
                invariants_hold = False
            if work["pingers"] not in PINGERS_ALLOWED:
                invariants_hold = False
            # mesh chains spans the magnitude buckets (small low .. large high).
            # ttl is drawn from the same range but then trimmed by clamp_ttl to
            # keep the run within the time ceiling, so a high-chains config can land
            # below the bucket floor (down to 0) -- only its upper bound is the
            # bucket max.
            if not (1500 <= work["chains"] <= 34000):
                invariants_hold = False
            if not (0 <= work["ttl"] <= 34000):
                invariants_hold = False
        elif kind == "cyclic":
            if any(k in work for k in
                   ("pingers", "producers", "messages", "apply-every")):
                invariants_hold = False
            if not (cyclic_gen_lo <= work["generations"] <= cyclic_gen_hi):
                invariants_hold = False
            if work["group"] not in common.CYCLIC_GROUPS:
                invariants_hold = False
            if not (1 <= work["chains"] <= cyclic_chains_hi):
                invariants_hold = False
            if not (1 <= work["ttl"] <= cyclic_ttl_hi):
                invariants_hold = False
        elif kind == "backpressure":
            # No chains/ttl (they live on the chain-based variants) and no
            # pingers/generations/group.
            if any(k in work for k in
                   ("pingers", "generations", "group", "chains", "ttl")):
                invariants_hold = False
            if work["producers"] not in common.BACKPRESSURE_PRODUCERS:
                invariants_hold = False
            if not (bp_msg_lo <= work["messages"] <= bp_msg_hi):
                invariants_hold = False
            if work["apply-every"] not in common.BACKPRESSURE_APPLY_EVERY:
                invariants_hold = False
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
    # Ceiling guard: assert the THEORETICAL worst case (the product of the bound
    # maxima), NOT the sampled worst over the 300 seeds -- a narrow bucket bump can
    # push the theoretical max over a ceiling without any sample reaching it (the
    # largest producers and largest messages need not co-occur), so a sampled check
    # would miss it. The product of the constants always trips on an unsafe bump.
    cyclic_theoretical = (max(common.CYCLIC_GROUPS)
                          * common.CYCLIC_GENERATION_BUCKETS["large"][1])
    bp_theoretical = (max(common.BACKPRESSURE_PRODUCERS)
                      * common.BACKPRESSURE_MESSAGE_BUCKETS["large"][1])
    check("cyclic theoretical worst-case workers within the memory ceiling",
          cyclic_theoretical <= CYCLIC_WORKER_CEILING)
    check("backpressure theoretical worst-case messages within the ceiling",
          bp_theoretical <= BACKPRESSURE_MESSAGE_CEILING)
    check("payload-mode covers {forward, fresh}",
          mode_seen == {"forward", "fresh"})
    check("ponymaxthreads spans the full [1, THREADS] range",
          maxthreads_seen == set(range(1, THREADS + 1)))


def test_resolve_config_draws_all_workloads():
    # Swarm coverage: over many seeds all three workload kinds must appear, or one
    # of the cycle-detector / backpressure / mesh paths would go untested in a soak.
    kinds = set()
    for master in range(0, 300):
        kinds.add(normal.resolve_config(master, THREADS)["workload"]["workload"])
    check("normal mode draws all of mesh, cyclic, backpressure",
          kinds == {"mesh", "cyclic", "backpressure"})


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
    # magnitude buckets; a CYCLIC run draws generations from its own buckets; a
    # BACKPRESSURE run draws messages from its own buckets. Over many seeds each
    # magnitude knob must reach all three of its buckets.
    nb = common.NORMAL_SIZE_BUCKETS
    gb = common.CYCLIC_GENERATION_BUCKETS
    mb = common.BACKPRESSURE_MESSAGE_BUCKETS
    mesh_chains = {"small": 0, "medium": 0, "large": 0}
    mesh_ttl = {"small": 0, "medium": 0, "large": 0}
    cyclic_gen = {"small": 0, "medium": 0, "large": 0}
    bp_messages = {"small": 0, "medium": 0, "large": 0}

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
        elif w["workload"] == "cyclic":
            cyclic_gen[bucket_of(w["generations"], gb)] += 1
        else:
            bp_messages[bucket_of(w["messages"], mb)] += 1
    check("mesh chains reaches all three magnitude buckets",
          all(c > 0 for c in mesh_chains.values()))
    check("mesh ttl reaches all three magnitude buckets",
          all(c > 0 for c in mesh_ttl.values()))
    check("cyclic generations reaches all three magnitude buckets",
          all(c > 0 for c in cyclic_gen.values()))
    check("backpressure messages reaches all three magnitude buckets",
          all(c > 0 for c in bp_messages.values()))


def test_resolve_config_draws_single_pinger():
    # pingers=1 must be drawn for some MESH run: it exercises the ORCA self-send
    # path (bounded since PR #5594), so the soak keeps a standing stress on that fix.
    one_seen = False
    for master in range(0, 500):
        w = normal.resolve_config(master, THREADS)["workload"]
        if (w["workload"] == "mesh") and (w["pingers"] == 1):
            one_seen = True
    check("normal mode draws a mesh run with pingers=1", one_seen)


def test_resolve_config_mesh_run_time_bounded():
    # Run time is bounded by trimming a mesh config's ttl: no mesh config's estimated
    # single-thread time may exceed the ceiling, and a high-chains FORWARD run (the
    # shape that used to take hours) must still be drawn -- just with its ttl trimmed
    # so it fits. Coverage of the dimension is preserved; only the can't-finish corner
    # (high chains AND high ttl together) is dropped.
    ceiling = common.RUN_TIME_CEILING_SECONDS
    large_lo = common.NORMAL_SIZE_BUCKETS["large"][0]
    small_lo = common.NORMAL_SIZE_BUCKETS["small"][0]
    over = high_chains_forward = bound_bit = False
    for master in range(0, 600):
        w = normal.resolve_config(master, THREADS)["workload"]
        if w["workload"] != "mesh":
            continue
        if common.est_mesh_seconds(w["chains"], w["ttl"], w["payload-mode"],
                                   w["payload"], w["payload-size"]) > ceiling:
            over = True
        if w["payload-mode"] == "forward" and w["chains"] >= large_lo:
            high_chains_forward = True
            # At large chains the forward budget caps ttl well below the small
            # bucket's floor, so a clamped run lands below it -- evidence the bound
            # actually bit, not just that high chains happened to draw a small ttl.
            if w["ttl"] < small_lo:
                bound_bit = True
    check("no mesh config's estimated run time exceeds the ceiling", not over)
    check("high-chains forward mesh runs are still drawn (coverage preserved)",
          high_chains_forward)
    check("the run-time bound actually trims a high-chains forward run", bound_bit)


def main():
    test_resolve_config_mesh_golden()
    test_resolve_config_cyclic_golden()
    test_resolve_config_backpressure_golden()
    test_resolve_config_never_sets_systematic_seed()
    test_ponynoblock_is_a_swarm_knob()
    test_resolve_config_deterministic_and_bounded()
    test_resolve_config_draws_all_workloads()
    test_resolve_config_swarm_omission()
    test_resolve_config_magnitude_buckets()
    test_resolve_config_draws_single_pinger()
    test_resolve_config_mesh_run_time_bounded()
    if FAILURES:
        print("%d failure(s): %s" % (len(FAILURES), ", ".join(FAILURES)))
        sys.exit(1)
    print("all orchestrate_normal_test checks passed")


if __name__ == "__main__":
    main()
