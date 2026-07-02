#!/usr/bin/env python3
"""Unit tests for orchestrate_normal.py's resolve_config (the normal-mode config
draw). Self-contained (no pytest): `python3 ..._test.py`, exits 0/1.

Normal mode is non-reproducible at runtime, but a seed still maps to a stable
*config* -- the per-kind goldens below pin that draw order. The two things that
distinguish normal from systematic are asserted explicitly: no systematic seed is
ever set, and ponynoblock is a swarm knob (drawn, not forced). Normal mode draws
one of five workload kinds (`mesh`, `cyclic`, `backpressure`, `iso`, or
`actorref`); the per-kind shape, the cyclic memory ceiling, the backpressure
message ceiling, the iso chains/ttl burst ceilings, and the actorref chains ceiling
are checked here too.
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
# Hardcoded calibrated ceilings for the iso workload. Its memory bound is the
# ACQUIRE-FLOOD = chains * ttl * node_count; the per-run chains draw is CLAMPED so
# this stays <= ISO_ACQUIRE_BUDGET (see stress_common). These ceilings pin the
# calibrated safe limits: the budget itself (worst ~160 MB at K=96000), and the max
# graph node count (the depth/breadth maxima -- the per-hop acquire multiplier).
# Bumping either past these forces a memory re-measure rather than slipping into
# untested territory (the OOM cliff is non-monotonic near the edge).
ISO_ACQUIRE_CEILING = 96000
ISO_NODE_CEILING = 15


def check(name, condition):
    if condition:
        print("ok   - " + name)
    else:
        print("FAIL - " + name)
        FAILURES.append(name)


def test_resolve_config_mesh_golden():
    # Pin a MESH config: the normal-mode seed-stability guard for the mesh emit
    # shape (pingers + chains + ttl, no generations/group/producers). Distinct from
    # the systematic golden -- normal draws the workload kind first (now 5-way), then
    # bucketed chains/ttl (a mesh run's ttl then trimmed to the run-time ceiling), a
    # freely-drawn payload, and ponynoblock as a swarm knob, and carries no
    # systematic seed.
    # (This intentionally differs from any earlier-arity normal golden -- the 5-way
    # kind roll remaps every historical seed via the reweighted thresholds; the
    # actorref addition draws no new cargo, so only the kind-roll thresholds moved.)
    # Seed 1 rolls mesh.
    expected = {
        "master_seed": 1,
        "runtime": {
            "ponygcfactor": 1.05,
            "ponygcinitial": 16,
            "ponymaxthreads": 1,
        },
        "workload": {
            "chains": 7886,
            "payload": "string",
            "payload-mode": "fresh",
            "payload-size": 8,
            "pingers": 8,
            "seed": 2570779959355939992,
            "ttl": 19020,
            "workload": "mesh",
        },
    }
    check("resolve_config(1, 8) matches the pinned mesh golden config",
          normal.resolve_config(1, THREADS) == expected)


def test_resolve_config_cyclic_golden():
    # Pin a CYCLIC config too (the mesh golden alone never checks the cyclic emit
    # shape: generations/group/chains/ttl present, no pingers/producers). Seed 9
    # rolls cyclic (the 4-way weight change remapped it from seed 5).
    expected = {
        "master_seed": 9,
        "runtime": {
            "ponymaxthreads": 7,
            "ponynoblock": True,
            "ponynoscale": True,
        },
        "workload": {
            "chains": 5,
            "generations": 1368,
            "group": 4,
            "payload": "u64",
            "payload-mode": "fresh",
            "payload-size": 1,
            "seed": 7931586630182256735,
            "ttl": 10,
            "workload": "cyclic",
        },
    }
    check("resolve_config(9, 8) matches the pinned cyclic golden config",
          normal.resolve_config(9, THREADS) == expected)


def test_resolve_config_backpressure_golden():
    # Pin a BACKPRESSURE config: it has producers/messages/apply-every and NO
    # chains/ttl/pingers/generations/group -- the emit shape that proves the variant
    # carries only its own fields. Seed 5 rolls backpressure (remapped from seed 0).
    expected = {
        "master_seed": 5,
        "runtime": {
            "ponycdinterval": 1000,
            "ponygcfactor": 1.05,
            "ponymaxthreads": 8,
            "ponynoblock": True,
            "ponynoscale": True,
        },
        "workload": {
            "apply-every": 1000,
            "messages": 230576,
            "payload": "string",
            "payload-mode": "forward",
            "payload-size": 1024,
            "producers": 128,
            "seed": 1674455568713221789,
            "workload": "backpressure",
        },
    }
    check("resolve_config(5, 8) matches the pinned backpressure golden config",
          normal.resolve_config(5, THREADS) == expected)


def test_resolve_config_iso_golden():
    # Pin an ISO config: it has pingers/chains/ttl + its OWN cargo knobs
    # (node-size/node-depth/node-breadth) and NO val payload and NO
    # generations/group/producers/messages/apply-every -- the emit shape that proves
    # the variant carries only its own fields and picks its own cargo set instead of
    # the shared payload. Seed 0 rolls iso; chains is clamped to the graph's
    # acquire-flood cap (depth 4, breadth 1 = 4 nodes, cap 1500).
    expected = {
        "master_seed": 0,
        "runtime": {
            "ponymaxthreads": 5,
            "ponynoblock": True,
        },
        "workload": {
            "chains": 1500,
            "node-breadth": 1,
            "node-depth": 4,
            "node-size": 64,
            "pingers": 16,
            "seed": 2686188150644990173,
            "ttl": 7,
            "workload": "iso",
        },
    }
    check("resolve_config(0, 8) matches the pinned iso golden config",
          normal.resolve_config(0, THREADS) == expected)


def test_resolve_config_actorref_golden():
    # Pin an ACTORREF config: pingers/chains/ttl and NO val payload and NO
    # generations/group/producers/messages/apply-every and NO iso cargo -- its cargo is
    # a fresh actor tag per chain, built in the engine. Seed 2 rolls actorref. chains is
    # drawn from its own magnitude bucket (no clamp -- memory tracks chains and stays
    # far under the cap); ttl from its small bucket (ttl >= 1).
    expected = {
        "master_seed": 2,
        "runtime": {
            "ponygcfactor": 2.0,
            "ponygcinitial": 0,
            "ponymaxthreads": 7,
            "ponynoblock": True,
            "ponynoscale": True,
        },
        "workload": {
            "chains": 11523,
            "pingers": 2,
            "seed": 10172212549529217571,
            "ttl": 8,
            "workload": "actorref",
        },
    }
    check("resolve_config(2, 8) matches the pinned actorref golden config",
          normal.resolve_config(2, THREADS) == expected)


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

    # Invariants over all five workload kinds: each carries ONLY its own shape
    # fields (mesh: pingers+chains+ttl; cyclic: generations+group+chains+ttl;
    # backpressure: producers+messages+apply-every and NO chains/ttl; iso:
    # pingers+chains+ttl + its OWN cargo knobs node-size/node-depth/node-breadth and
    # NO val payload; actorref: pingers+chains+ttl and NO val payload). This is the
    # structural check that the per-kind emit never leaks a field from another kind.
    # (iso's lack of payload is itself checked: the
    # shared payload reads below are guarded for iso.)
    cyclic_gen_lo = common.CYCLIC_GENERATION_BUCKETS["small"][0]
    cyclic_gen_hi = common.CYCLIC_GENERATION_BUCKETS["large"][1]
    cyclic_chains_hi = common.CYCLIC_CHAINS_BUCKETS["large"][1]
    cyclic_ttl_hi = common.CYCLIC_TTL_BUCKETS["large"][1]
    bp_msg_lo = common.BACKPRESSURE_MESSAGE_BUCKETS["small"][0]
    bp_msg_hi = common.BACKPRESSURE_MESSAGE_BUCKETS["large"][1]
    iso_ttl_lo = common.ISO_TTL_BUCKETS["small"][0]
    iso_ttl_hi = common.ISO_TTL_BUCKETS["large"][1]
    ar_chains_lo = common.ACTORREF_CHAINS_BUCKETS["small"][0]
    ar_chains_hi = common.ACTORREF_CHAINS_BUCKETS["large"][1]
    ar_ttl_lo = common.ACTORREF_TTL_BUCKETS["small"][0]
    ar_ttl_hi = common.ACTORREF_TTL_BUCKETS["large"][1]
    # Every other kind's unique fields, which the iso emit must never carry, and the
    # iso cargo knobs, which the OTHER kinds must never carry.
    iso_cargo = ("node-size", "node-depth", "node-breadth")
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
                    "apply-every") + iso_cargo):
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
                   ("pingers", "producers", "messages", "apply-every")
                   + iso_cargo):
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
            # pingers/generations/group and no iso cargo knobs.
            if any(k in work for k in
                   ("pingers", "generations", "group", "chains", "ttl")
                   + iso_cargo):
                invariants_hold = False
            if work["producers"] not in common.BACKPRESSURE_PRODUCERS:
                invariants_hold = False
            if not (bp_msg_lo <= work["messages"] <= bp_msg_hi):
                invariants_hold = False
            if work["apply-every"] not in common.BACKPRESSURE_APPLY_EVERY:
                invariants_hold = False
        elif kind == "iso":
            # pingers+chains+ttl + node-size/depth/breadth; NO val payload and no
            # other kind's fields.
            if any(k in work for k in
                   ("generations", "group", "producers", "messages",
                    "apply-every", "payload", "payload-size", "payload-mode")):
                invariants_hold = False
            if work["pingers"] not in PINGERS_ALLOWED:
                invariants_hold = False
            if work["node-size"] not in common.ISO_NODE_SIZES:
                invariants_hold = False
            if work["node-depth"] not in common.ISO_DEPTHS:
                invariants_hold = False
            if work["node-breadth"] not in common.ISO_BREADTHS:
                invariants_hold = False
            if not (iso_ttl_lo <= work["ttl"] <= iso_ttl_hi):
                invariants_hold = False
            # chains is clamped to the graph's acquire-flood cap, so every iso run
            # must satisfy chains <= cap AND the budget (chains * ttl_max *
            # node_count <= ISO_ACQUIRE_BUDGET) -- the memory-safe envelope.
            nd = work["node-depth"]
            nb = work["node-breadth"]
            cap = common.iso_chains_cap(nd, nb)
            nodes = common.iso_node_count(nd, nb)
            if not (1 <= work["chains"] <= cap):
                invariants_hold = False
            if (work["chains"] * common.ISO_TTL_MAX * nodes
                    > common.ISO_ACQUIRE_BUDGET):
                invariants_hold = False
        elif kind == "actorref":
            # pingers+chains+ttl and NO val payload; no other kind's fields and no iso
            # cargo. chains from the actorref magnitude bucket, ttl from its small
            # bucket (ttl >= 1 -- a zero-hop (ttl=0) chain drives no acquire).
            if any(k in work for k in
                   ("generations", "group", "producers", "messages",
                    "apply-every", "payload", "payload-size", "payload-mode")
                   + iso_cargo):
                invariants_hold = False
            if work["pingers"] not in PINGERS_ALLOWED:
                invariants_hold = False
            if not (ar_chains_lo <= work["chains"] <= ar_chains_hi):
                invariants_hold = False
            if not (ar_ttl_lo <= work["ttl"] <= ar_ttl_hi):
                invariants_hold = False
        else:
            invariants_hold = False
        # Payload is shared by mesh/cyclic/backpressure but NOT iso/actorref (they pick
        # their own cargo), so guard the shared reads for those.
        if kind not in ("iso", "actorref"):
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
    # iso's memory bound is the acquire-flood budget (the chains clamp keeps every
    # run within it -- asserted per-config above). Pin the calibrated budget and the
    # max graph node count; bumping either forces a memory re-measure.
    iso_max_nodes = max(common.iso_node_count(d, b)
                        for d in common.ISO_DEPTHS for b in common.ISO_BREADTHS)
    check("iso calibrated acquire budget within the ceiling",
          common.ISO_ACQUIRE_BUDGET <= ISO_ACQUIRE_CEILING)
    check("iso max graph node count within the ceiling",
          iso_max_nodes <= ISO_NODE_CEILING)
    # actorref peak RSS tracks CHAINS (flat in ttl -- calibrated), so the guard is on
    # the chains bucket max; bumping it forces a memory re-measure on the normal build.
    check("actorref theoretical worst-case chains within the memory ceiling",
          common.ACTORREF_CHAINS_BUCKETS["large"][1]
          <= common.ACTORREF_CHAINS_CEILING)
    check("payload-mode covers {forward, fresh}",
          mode_seen == {"forward", "fresh"})
    check("ponymaxthreads spans the full [1, THREADS] range",
          maxthreads_seen == set(range(1, THREADS + 1)))


def test_resolve_config_draws_all_workloads():
    # Swarm coverage: over many seeds all five workload kinds must appear, or one of
    # the cycle-detector / backpressure / iso / actorref / mesh paths would go untested
    # in a soak.
    kinds = set()
    for master in range(0, 300):
        kinds.add(normal.resolve_config(master, THREADS)["workload"]["workload"])
    check("normal mode draws all of mesh, cyclic, backpressure, iso, actorref",
          kinds == {"mesh", "cyclic", "backpressure", "iso", "actorref"})


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
    # BACKPRESSURE run draws messages from its own buckets. ISO's magnitude is in its
    # ttl (bucketed) AND its GRAPH SHAPE (node count): its chains is clamped to the
    # graph's acquire cap, so chains is not an independent magnitude knob (a large
    # draw on a big graph clamps down) -- the graph node count is the iso magnitude
    # dimension instead. Each branch reads only its own kind's field, so iso needs
    # its own arm (the backpressure `messages` it lacks would KeyError).
    nb = common.NORMAL_SIZE_BUCKETS
    gb = common.CYCLIC_GENERATION_BUCKETS
    mb = common.BACKPRESSURE_MESSAGE_BUCKETS
    itb = common.ISO_TTL_BUCKETS
    acb = common.ACTORREF_CHAINS_BUCKETS
    mesh_chains = {"small": 0, "medium": 0, "large": 0}
    mesh_ttl = {"small": 0, "medium": 0, "large": 0}
    cyclic_gen = {"small": 0, "medium": 0, "large": 0}
    bp_messages = {"small": 0, "medium": 0, "large": 0}
    iso_ttl = {"small": 0, "medium": 0, "large": 0}
    iso_node_counts = set()
    actorref_chains = {"small": 0, "medium": 0, "large": 0}

    def bucket_of(val, buckets):
        if val <= buckets["small"][1]:
            return "small"
        if val <= buckets["medium"][1]:
            return "medium"
        return "large"

    for master in range(0, 400):
        w = normal.resolve_config(master, THREADS)["workload"]
        wl = w["workload"]
        if wl == "mesh":
            mesh_chains[bucket_of(w["chains"], nb)] += 1
            mesh_ttl[bucket_of(w["ttl"], nb)] += 1
        elif wl == "cyclic":
            cyclic_gen[bucket_of(w["generations"], gb)] += 1
        elif wl == "backpressure":
            bp_messages[bucket_of(w["messages"], mb)] += 1
        elif wl == "actorref":
            actorref_chains[bucket_of(w["chains"], acb)] += 1
        else:  # iso
            iso_ttl[bucket_of(w["ttl"], itb)] += 1
            iso_node_counts.add(
                common.iso_node_count(w["node-depth"], w["node-breadth"]))
    check("mesh chains reaches all three magnitude buckets",
          all(c > 0 for c in mesh_chains.values()))
    check("mesh ttl reaches all three magnitude buckets",
          all(c > 0 for c in mesh_ttl.values()))
    check("cyclic generations reaches all three magnitude buckets",
          all(c > 0 for c in cyclic_gen.values()))
    check("backpressure messages reaches all three magnitude buckets",
          all(c > 0 for c in bp_messages.values()))
    check("iso ttl reaches all three magnitude buckets",
          all(c > 0 for c in iso_ttl.values()))
    # iso graph shape spans flat (1 node) through the max tree (15 nodes).
    iso_max_nodes = max(common.iso_node_count(d, b)
                        for d in common.ISO_DEPTHS for b in common.ISO_BREADTHS)
    check("iso graph reaches both the single node and the max tree",
          (1 in iso_node_counts) and (iso_max_nodes in iso_node_counts))
    check("actorref chains reaches all three magnitude buckets",
          all(c > 0 for c in actorref_chains.values()))


def test_resolve_config_draws_single_pinger():
    # pingers=1 must be drawn for some MESH run AND some ISO run AND some ACTORREF run:
    # it exercises the ORCA self-send path (bounded since PR #5594), so the soak keeps a
    # standing stress on that fix -- the iso run additionally self-sends an iso nested
    # graph (exercising the pin with a moved mutable subgraph, see
    # self-send-object-pinning), and the actorref run self-sends a foreign actor `tag`
    # (the highest actor-ref acquire rate, measured memory-safe -- no #5594-style flood,
    # since fresh referents move hop-to-hop and are collected).
    mesh_one_seen = False
    iso_one_seen = False
    actorref_one_seen = False
    for master in range(0, 500):
        w = normal.resolve_config(master, THREADS)["workload"]
        # cyclic/backpressure have no `pingers`, so .get() guards the read.
        if w.get("pingers") == 1:
            if w["workload"] == "mesh":
                mesh_one_seen = True
            elif w["workload"] == "iso":
                iso_one_seen = True
            elif w["workload"] == "actorref":
                actorref_one_seen = True
    check("normal mode draws a mesh run with pingers=1", mesh_one_seen)
    check("normal mode draws an iso run with pingers=1", iso_one_seen)
    check("normal mode draws an actorref run with pingers=1", actorref_one_seen)


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
    test_resolve_config_iso_golden()
    test_resolve_config_actorref_golden()
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
