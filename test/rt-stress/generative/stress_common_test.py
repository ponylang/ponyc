#!/usr/bin/env python3
"""Unit tests for the pure, mode-agnostic pieces of stress_common.py.

Self-contained (no pytest): runnable as
`python3 test/rt-stress/generative/stress_common_test.py`, exits 0 on pass / 1 on
failure. Picked up by test-rt-stress.yml's `*_test.py` discovery.
"""
import os
import random
import shutil
import sys
import time
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


# The exact field set each kind emits: the engine must get precisely the rolled
# kind's flags and none of another kind's (draw_systematic_workload emits only
# those). `seed` + `workload` are always present; `payload-mode` is NEVER here (the
# driver draws it later).
_SYSTEMATIC_KIND_FIELDS = {
    "mesh":   {"seed", "workload", "pingers", "chains", "ttl", "payload",
               "payload-size"},
    "cyclic": {"seed", "workload", "generations", "group", "chains", "ttl",
               "payload", "payload-size"},
    "backpressure": {"seed", "workload", "producers", "messages", "apply-every",
                     "payload", "payload-size"},
    "iso":    {"seed", "workload", "pingers", "chains", "ttl", "node-size",
               "node-depth", "node-breadth"},
    "actorref": {"seed", "workload", "pingers", "chains", "ttl"},
}


def test_draw_systematic_workload_contract():
    # CRITICAL contract: draw_systematic_workload returns the passed program seed, a
    # drawn kind, and ONLY the rolled kind's shape fields -- payload-mode is NOT here
    # (the systematic driver draws it after its runtime knobs). Pin Random(0),999: a
    # reordered/narrowed/added/removed draw changes this and would silently remap
    # every systematic seed (breaking historical --replay).
    work = common.draw_systematic_workload(random.Random(0), 999)
    check("draw_systematic_workload carries the passed program seed",
          work["seed"] == 999)
    check("draw_systematic_workload emits a workload kind key",
          work.get("workload") in ("mesh", "cyclic", "backpressure", "iso",
                                    "actorref"))
    check("draw_systematic_workload does NOT draw payload-mode",
          "payload-mode" not in work)
    check("draw_systematic_workload(Random(0), 999) matches the pinned draw",
          work == {"seed": 999, "workload": "iso", "pingers": 64, "chains": 184,
                   "ttl": 7, "node-size": 256, "node-depth": 3, "node-breadth": 1})


def test_draw_systematic_workload_coverage():
    kinds_seen = set()
    # Track the backpressure regimes the draw must actually PRODUCE, not just stay
    # within: a membership check still passes if the constant were narrowed to drop a
    # documented regime, so assert presence too -- the same guard the gcinitial=0 draw
    # and the max-threads span get (test_draw_swarm_knobs, draw_max_threads).
    bp_producers_seen = set()
    bp_apply_seen = set()
    invariants = True
    for master in range(0, 500):
        w = common.draw_systematic_workload(random.Random(master), 1)
        kind = w["workload"]
        kinds_seen.add(kind)
        # Exactly the rolled kind's field set -- no missing/extra/contradictory flag.
        if set(w.keys()) != _SYSTEMATIC_KIND_FIELDS[kind]:
            invariants = False
        # chains/ttl within the kind's declared range, for the kinds that carry them.
        # backpressure has neither (it draws them then drops them, emitting no
        # chains/ttl key), so it is excluded from this range check. iso chains stays
        # <= its hi even after the acquire clamp, which can only reduce it.
        if kind != "backpressure":
            (clo, chi), (tlo, thi) = common.SYSTEMATIC_WORKLOAD_RANGES[kind]
            if not (clo <= w["chains"] <= chi) or not (tlo <= w["ttl"] <= thi):
                invariants = False
        if kind == "cyclic":
            if not (1 <= w["generations"]
                    <= common.SYSTEMATIC_CYCLIC_GENERATION_MAX):
                invariants = False
            if w["group"] not in common.CYCLIC_GROUPS:
                invariants = False
        elif kind == "backpressure":
            if w["producers"] not in common.SYSTEMATIC_BACKPRESSURE_PRODUCERS:
                invariants = False
            if not (1 <= w["messages"]
                    <= common.SYSTEMATIC_BACKPRESSURE_MESSAGES_MAX):
                invariants = False
            if w["apply-every"] not in common.SYSTEMATIC_BACKPRESSURE_APPLY_EVERY:
                invariants = False
            bp_producers_seen.add(w["producers"])
            bp_apply_seen.add(w["apply-every"])
        elif kind == "iso":
            if w["node-size"] not in common.ISO_NODE_SIZES:
                invariants = False
            if w["node-depth"] not in common.ISO_DEPTHS:
                invariants = False
            if w["node-breadth"] not in common.ISO_BREADTHS:
                invariants = False
            if w["pingers"] not in common.NORMAL_PINGERS:
                invariants = False
            # The acquire clamp must hold the memory/acquire invariant per config:
            # chains * ttl * node_count <= ISO_ACQUIRE_BUDGET.
            nodes = common.iso_node_count(w["node-depth"], w["node-breadth"])
            if (w["chains"] * w["ttl"] * nodes) > common.ISO_ACQUIRE_BUDGET:
                invariants = False
        elif kind == "actorref":
            if w["pingers"] not in common.NORMAL_PINGERS:
                invariants = False
        else:                                              # mesh
            if w["pingers"] not in common.NORMAL_PINGERS:
                invariants = False
        # Every kind except iso and actorref carries the val payload (they omit it).
        if (kind not in ("iso", "actorref")
                and (w["payload"] not in ("string", "u64")
                     or w["payload-size"] not in common.STRING_SIZES)):
            invariants = False
    check("draw_systematic_workload covers mesh, cyclic, backpressure, iso, actorref",
          kinds_seen == {"mesh", "cyclic", "backpressure", "iso", "actorref"})
    check("draw_systematic_workload emits exactly each kind's fields, all in range",
          invariants)
    # The two backpressure regimes the draw EXISTS to exercise are both actually
    # produced: producers=1 (the single-producer baseline, run nowhere else -- normal
    # mode starts at 16) and some producers>=4 (the multi-producer arrival race the
    # per-work ORDER_SIG fold fingerprints). Narrowing SYSTEMATIC_BACKPRESSURE_PRODUCERS
    # to drop either would still pass the membership check above but fails here.
    check("backpressure draws the single-producer baseline (producers=1)",
          1 in bp_producers_seen)
    check("backpressure draws a multi-producer race case (producers>=4)",
          any(p >= 4 for p in bp_producers_seen))
    # apply_every=1 is the maximal mute/unmute churn regime -- the strongest determinism
    # stress; assert it is actually drawn, not just permitted.
    check("backpressure draws the maximal-churn regime (apply_every=1)",
          1 in bp_apply_seen)

    # Fixed-consumption: the SAME rng call count whichever kind it rolls (it draws
    # every kind's shape unconditionally), so a future kind-dependent draw -- which
    # would remap seeds -- is caught. Same guard as draw_workload; compare the call
    # COUNT (a randint's bit consumption varies with its value, but the call sequence
    # does not).
    counts = []
    for kind in ("mesh", "cyclic", "backpressure", "iso", "actorref"):
        seed = next(
            s for s in range(0, 500)
            if common.draw_systematic_workload(random.Random(s), 1)["workload"]
            == kind)
        counter = _CallCountingRandom(seed)
        common.draw_systematic_workload(counter, 1)
        counts.append(counter.calls)
    check("draw_systematic_workload makes the same rng call count for every kind",
          (len(set(counts)) == 1) and (counts[0] > 0))

    # Cap ceilings: guard the PRODUCT that drives cost (parity with normal's
    # cyclic_theoretical), so a CYCLIC_GROUPS or ISO_TTL_MAX bump also trips a
    # serialized re-measure -- not just a generation/chains-cap bump. The per-config
    # iso acquire-flood (the 15-node corner) is asserted in the loop above.
    check("systematic cyclic worst-case workers within the ceiling",
          (common.SYSTEMATIC_CYCLIC_GENERATION_MAX * max(common.CYCLIC_GROUPS))
          < SYSTEMATIC_CYCLIC_WORKER_CEILING)
    check("systematic iso worst-case 1-node messages within the ceiling",
          (common.SYSTEMATIC_ISO_CHAINS_MAX * common.ISO_TTL_MAX)
          < SYSTEMATIC_ISO_MESSAGE_CEILING)
    check("systematic backpressure worst-case messages within the ceiling",
          (max(common.SYSTEMATIC_BACKPRESSURE_PRODUCERS)
           * common.SYSTEMATIC_BACKPRESSURE_MESSAGES_MAX)
          < SYSTEMATIC_BACKPRESSURE_MESSAGE_CEILING)
    check("systematic actorref worst-case hops within the ceiling",
          (common.SYSTEMATIC_ACTORREF_CHAINS_MAX * (common.ISO_TTL_MAX + 1))
          < SYSTEMATIC_ACTORREF_MESSAGE_CEILING)
    # iso's ttl floor is load-bearing coverage: a ttl=0 (zero-hop) chain is delivered
    # once and terminates without a hand-off, so it drives none of the per-hop
    # foreign-mutable acquire-flood (chains*ttl*node_count) the workload exists to
    # exercise, while conservation still passes. Same reasoning as actorref below;
    # assert the floor directly.
    check("systematic iso ttl floor is >= 1 (ttl 0 = zero-hop = no acquire)",
          common.SYSTEMATIC_WORKLOAD_RANGES["iso"][1][0] >= 1)
    # actorref's ttl floor is load-bearing coverage: a ttl=0 (zero-hop) chain injects
    # but never forwards, so it drives ZERO acquire_actor -- the workload's whole point
    # -- while conservation still passes (expected == chains). The engine sets no CLI
    # ttl floor, so this range is the sole guarantor. Assert the floor directly (not via
    # a drawn value, which would be tautological against the range under test).
    check("systematic actorref ttl floor is >= 1 (ttl 0 = zero-hop = no acquire)",
          common.SYSTEMATIC_WORKLOAD_RANGES["actorref"][1][0] >= 1)
    # chains=0 is silent uselessness: a run that injects zero chains still conserves
    # (received == sent == expected == 0), so it scores green while testing nothing --
    # the soak's pass/fail can't tell it from a real run. Every chains-bearing kind
    # (mesh/cyclic/iso/actorref; backpressure draws chains but ignores them) must floor
    # chains at >= 1. Assert the range floors directly (a drawn value is tautological).
    check("systematic chains floor is >= 1 for every chains-bearing kind",
          all(common.SYSTEMATIC_WORKLOAD_RANGES[k][0][0] >= 1
              for k in ("mesh", "cyclic", "iso", "actorref")))


def test_draw_swarm_knobs():
    # Omission IS the mechanism: each of the four optional knobs must appear in
    # some runs and be absent in others. They share identical `if rng.random() <
    # 0.5` logic, so test all four (a regression in any one would otherwise pass).
    # ponynoblock is NOT one of them -- draw_swarm_knobs must never add it.
    omittable = ["ponynoscale", "ponygcinitial", "ponygcfactor", "ponycdinterval"]
    counts = dict.fromkeys(omittable, 0)
    ponynoblock_added = False
    gcinitial_values = set()
    for master in range(0, 300):
        runtime = {}
        common.draw_swarm_knobs(random.Random(master), runtime)
        if "ponynoblock" in runtime:
            ponynoblock_added = True
        if "ponygcinitial" in runtime:
            gcinitial_values.add(runtime["ponygcinitial"])
        for knob in omittable:
            if knob in runtime:
                counts[knob] += 1
    for knob in omittable:
        check("swarm omission: %s sometimes present" % knob, counts[knob] > 0)
        check("swarm omission: %s sometimes absent" % knob, counts[knob] < 300)
    check("draw_swarm_knobs never adds ponynoblock", not ponynoblock_added)
    # 0 is the deliberate constant-GC stress point (a 1-byte threshold). Unlike
    # the other swarm knobs we pin its presence in the draw -- without this a
    # refactor could silently drop the regime the value exists to exercise.
    check("swarm draws the gcinitial=0 constant-GC stress point",
          0 in gcinitial_values)


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
    # Size is drawn freely from the full set now -- run time is bounded by clamp_ttl
    # (which trims ttl), not by restricting which sizes a big run may pick. Every
    # size, kind, and mode must be reachable across seeds.
    sizes, kinds, modes = set(), set(), set()
    for master in range(0, 2000):
        k, s, m = common.draw_payload(random.Random(master))
        sizes.add(s)
        kinds.add(k)
        modes.add(m)
    check("draw_payload covers every string size", sizes == set(common.STRING_SIZES))
    check("draw_payload covers {string, u64}", kinds == {"string", "u64"})
    check("draw_payload covers {forward, fresh}", modes == {"forward", "fresh"})

    # Seed-stability contract: exactly three rng draws -- kind, mode, size -- in that
    # order, so draw_payload never remaps a downstream draw. Compare the rng state
    # after the call to a hand-rolled three-draw reference.
    stable = True
    for master in range(0, 300):
        rng = random.Random(master)
        common.draw_payload(rng)
        ref = random.Random(master)
        ref.choice(["string", "u64"])
        ref.choice(["forward", "fresh"])
        ref.random()
        if rng.getstate() != ref.getstate():
            stable = False
    check("draw_payload consumes exactly three rng draws (kind, mode, size)", stable)


def test_clamp_ttl():
    # The mesh run-time bound. Forward cost is quadratic in chains, fresh linear, so
    # est must separate them; clamp_ttl trims ttl so the estimate fits the ceiling.
    C = common.RUN_TIME_CEILING_SECONDS
    check("est: forward cost is >>(10x) fresh at high chains",
          common.est_mesh_seconds(30000, 1000, "forward", "u64", 1)
          > 10 * common.est_mesh_seconds(30000, 1000, "fresh", "u64", 1))

    # An in-budget config is returned unchanged.
    check("clamp_ttl leaves an in-budget config alone",
          common.clamp_ttl(2000, 1000, "fresh", "u64", 1) == 1000)

    # The original failing seed: forward u64, chains=31178, ttl=30244 (hours of run
    # time). It must clamp to a large-but-finite ttl whose estimate fits the ceiling.
    ct = common.clamp_ttl(31178, 30244, "forward", "u64", 256)
    check("clamp_ttl bounds the failing forward seed", ct < 30244)
    check("clamp_ttl leaves a usable ttl for the failing seed", ct > 0)
    check("clamp_ttl keeps the failing seed's estimate within the ceiling",
          common.est_mesh_seconds(31178, ct, "forward", "u64", 256) <= C)

    # Never raises ttl; floors at 0 (never negative) even at the most extreme config.
    over = common.clamp_ttl(34000, 34000, "forward", "string", 4096)
    check("clamp_ttl never raises ttl", over <= 34000)
    check("clamp_ttl floors at 0 (never negative)", over >= 0)

    # Every (mode, payload, size) at max chains AND max ttl clamps within the ceiling.
    worst_ok = True
    max_dim = common.NORMAL_SIZE_BUCKETS["large"][1]
    for mode in ("forward", "fresh"):
        for payload in ("u64", "string"):
            for size in common.STRING_SIZES:
                t = common.clamp_ttl(max_dim, max_dim, mode, payload, size)
                if common.est_mesh_seconds(max_dim, t, mode, payload, size) > C:
                    worst_ok = False
    check("clamp_ttl bounds every payload/size at max chains+ttl", worst_ok)


# The calibrated ceiling on generations*group (leaked actors in a CD-off run).
# Hardcoded, NOT derived from the CYCLIC_* constants, so bumping a generation
# bucket or adding a bigger group trips this guard and forces a re-measure (strict
# `<`, so an exact bump to the ceiling trips too). The real max today is 8000*16 =
# 128000 (~1.2 GB peak, fits under DEFAULT_MEM_LIMIT_MB with margin -- see the
# decision log / CYCLIC_* docstring).
CYCLIC_WORKER_CEILING = 130000


# The calibrated ceiling on producers*messages (total backpressure work).
# Hardcoded, NOT derived from the BACKPRESSURE_* constants, so bumping a producer
# count or a message bucket trips this guard and forces a time re-measure. Strict `<`
# (as with cyclic above), so an exact bump to the ceiling trips too. Real max today is
# 256*400000 = 102_400_000.
BACKPRESSURE_MESSAGE_CEILING = 110_000_000


# Calibrated iso ceilings. iso's memory bound is the ACQUIRE-FLOOD =
# chains * ttl * node_count; the per-run chains draw is CLAMPED so this stays within
# ISO_ACQUIRE_BUDGET (see stress_common). These pin the calibrated budget (worst
# ~160 MB at K=96000) and the max graph node count; bumping either forces a memory
# re-measure (the OOM cliff is non-monotonic near the edge).
ISO_ACQUIRE_CEILING = 96000
ISO_NODE_CEILING = 15


# Calibrated ceiling on actorref chains (peak RSS tracks max chains). Hardcoded, NOT
# derived from ACTORREF_CHAINS_BUCKETS, so bumping the chains bucket max trips this and
# forces a memory re-measure. Unlike the cyclic/backpressure/iso ceilings above -- which
# sit ~1x above their drawn max because their cost near that max is non-linear or
# time-bound, so any bump past the calibrated max warrants a fresh measurement --
# actorref's drawn max (34000 chains, ~100 MB) has ~40x headroom under the memory cap
# and its cost is LINEAR, so this is a looser 2x trigger (matching the 2x SYSTEMATIC
# ceilings below). Strict `<`, so an exact doubling of the chains bucket max reaches the
# ceiling and trips the re-measure, not only a bump strictly past it.
ACTORREF_CHAINS_CEILING = 34000 * 2


# Systematic-mode cyclic/backpressure/iso are TIME-bound: kept small so the serialized
# double-run soak's per-seed cost stays ~= the mesh-only cost it replaces. Guard the
# PRODUCT that drives cost (like normal's cyclic_theoretical above), so a bump to
# CYCLIC_GROUPS or ISO_TTL_MAX -- not just the generation/chains cap -- also trips a
# serialized re-measure. cyclic cost ~ generations*group workers; iso 1-node cost ~
# chains*ttl messages (the 15-node acquire-flood corner is separately bounded by
# ISO_ACQUIRE_BUDGET, asserted per-config in test_draw_systematic_workload_coverage);
# backpressure cost ~ producers*messages total work. Each ceiling is 2x the drawn-worst
# product, compared with a strict `<` -- the worst must stay UNDER 2x, so an exact
# doubling of a cap (e.g. generations 50->100) trips the re-measure trigger; a plain
# `<=` would let that exact-2x bump slip through. Calibrated single-run: cyclic gen 100
# * group 16 ~1.18s, iso chains 800 * ttl 16 ~0.77s, backpressure 64 * 600 ~1.0s -- all
# well under the systematic watchdog even run twice (each kind's drawn worst is ~half
# its ceiling).
SYSTEMATIC_CYCLIC_WORKER_CEILING = 100 * 16
SYSTEMATIC_ISO_MESSAGE_CEILING = 800 * 16
SYSTEMATIC_BACKPRESSURE_MESSAGE_CEILING = 64 * 600
# actorref cost ~ chains*(ttl+1) hops (each hop drives one actor-ref acquire/release).
# 2x the drawn-worst 400*(16+1) = 6800 hops; calibrated single-run ~0.5s, well under
# the watchdog even run twice.
SYSTEMATIC_ACTORREF_MESSAGE_CEILING = 400 * (16 + 1) * 2


class _CallCountingRandom(random.Random):
    """A Random that counts the high-level draw calls (random/randint/choice) made
    on it, delegating to the real implementation so the produced values are
    unchanged. Used to verify draw_workload makes the same number of calls whichever
    kind it rolls."""

    def __init__(self, seed):
        super().__init__(seed)
        self.calls = 0

    def random(self):
        self.calls += 1
        return super().random()

    def randint(self, a, b):
        self.calls += 1
        return super().randint(a, b)

    def choice(self, seq):
        self.calls += 1
        return super().choice(seq)


def test_draw_workload():
    check("draw_workload is deterministic",
          common.draw_workload(random.Random(7))
          == common.draw_workload(random.Random(7)))

    kinds = set()
    bounds_ok = True
    gen_lo = common.CYCLIC_GENERATION_BUCKETS["small"][0]
    gen_hi = common.CYCLIC_GENERATION_BUCKETS["large"][1]
    bp_msg_lo = common.BACKPRESSURE_MESSAGE_BUCKETS["small"][0]
    bp_msg_hi = common.BACKPRESSURE_MESSAGE_BUCKETS["large"][1]
    for master in range(0, 500):
        (workload, generations, group, producers, messages, apply_every,
         node_size, depth, breadth) = \
            common.draw_workload(random.Random(master))
        kinds.add(workload)
        if workload not in ("mesh", "cyclic", "backpressure", "iso", "actorref"):
            bounds_ok = False
        # EVERY kind's params are drawn for every roll (fixed rng consumption), so
        # they are always valid numbers in range regardless of the rolled kind --
        # checking them over all seeds is the evidence the draw never skips any.
        if not (gen_lo <= generations <= gen_hi):
            bounds_ok = False
        if group not in common.CYCLIC_GROUPS:
            bounds_ok = False
        if producers not in common.BACKPRESSURE_PRODUCERS:
            bounds_ok = False
        if not (bp_msg_lo <= messages <= bp_msg_hi):
            bounds_ok = False
        if apply_every not in common.BACKPRESSURE_APPLY_EVERY:
            bounds_ok = False
        if node_size not in common.ISO_NODE_SIZES:
            bounds_ok = False
        if depth not in common.ISO_DEPTHS:
            bounds_ok = False
        if breadth not in common.ISO_BREADTHS:
            bounds_ok = False
    check("draw_workload covers all five workload kinds",
          kinds == {"mesh", "cyclic", "backpressure", "iso", "actorref"})
    check("draw_workload all shape params always in range (drawn for every kind)",
          bounds_ok)
    # Ceiling guard: assert the THEORETICAL worst case (the product of the bound
    # maxima), NOT the sampled worst over the seeds. A narrow bucket bump can push
    # the theoretical max over a ceiling without any 500-seed sample reaching it
    # (the largest producers and the largest messages need not co-occur in a draw),
    # so a sampled check would miss it. The product of the constants always trips.
    cyclic_theoretical = (max(common.CYCLIC_GROUPS)
                          * common.CYCLIC_GENERATION_BUCKETS["large"][1])
    bp_theoretical = (max(common.BACKPRESSURE_PRODUCERS)
                      * common.BACKPRESSURE_MESSAGE_BUCKETS["large"][1])
    check("cyclic theoretical worst-case workers within the memory ceiling",
          cyclic_theoretical < CYCLIC_WORKER_CEILING)
    check("backpressure theoretical worst-case messages within the ceiling",
          bp_theoretical < BACKPRESSURE_MESSAGE_CEILING)
    # iso is governed by the acquire-flood budget (chains * ttl * node_count); the
    # per-run chains clamp (resolve_config) keeps every run within it. Pin the
    # calibrated budget and the max graph node count; bumping either forces a memory
    # re-measure. Also verify the clamp's max output (the 1-node cap) never exceeds
    # the pre-clamp chains bucket max, i.e. the cap is the binding limit.
    iso_max_nodes = max(common.iso_node_count(d, b)
                        for d in common.ISO_DEPTHS for b in common.ISO_BREADTHS)
    check("iso calibrated acquire budget within the ceiling",
          common.ISO_ACQUIRE_BUDGET <= ISO_ACQUIRE_CEILING)
    check("iso max graph node count within the ceiling",
          iso_max_nodes <= ISO_NODE_CEILING)
    check("iso 1-node chains cap stays within the acquire budget",
          (common.iso_chains_cap(1, 0) * common.ISO_TTL_MAX
           * common.iso_node_count(1, 0)) <= common.ISO_ACQUIRE_BUDGET)
    # actorref peak RSS tracks CHAINS (flat in ttl -- calibrated), so the guard is on
    # the chains bucket max; bumping it forces a memory re-measure on the normal build.
    check("actorref theoretical worst-case chains within the memory ceiling",
          common.ACTORREF_CHAINS_BUCKETS["large"][1]
          < ACTORREF_CHAINS_CEILING)
    # iso's ttl floor is load-bearing coverage (a ttl=0 zero-hop chain terminates
    # without a hand-off, driving none of the per-hop foreign-mutable acquire-flood,
    # while conservation still passes). Assert the bucket floor directly.
    check("normal iso ttl floor is >= 1 (ttl 0 = zero-hop = no acquire)",
          common.ISO_TTL_BUCKETS["small"][0] >= 1)
    # actorref's ttl floor is load-bearing coverage (a ttl=0 zero-hop chain drives zero
    # acquire_actor while conservation still passes). Assert the bucket floor directly.
    check("normal actorref ttl floor is >= 1 (ttl 0 = zero-hop = no acquire)",
          common.ACTORREF_TTL_BUCKETS["small"][0] >= 1)
    # chains=0 is silent uselessness (see the systematic test): a zero-chain run
    # conserves and scores green while testing nothing. Each chains-bearing normal
    # bucket floors chains at >= 1 (backpressure has none). Assert the floors directly.
    check("normal chains floor is >= 1 for every chains-bearing bucket",
          all(b["small"][0] >= 1 for b in (
              common.NORMAL_SIZE_BUCKETS, common.CYCLIC_CHAINS_BUCKETS,
              common.ISO_CHAINS_BUCKETS, common.ACTORREF_CHAINS_BUCKETS)))

    # Fixed-consumption contract: draw_workload must make the SAME sequence of rng
    # calls whichever kind it rolls (it draws every kind's shape unconditionally),
    # so a future kind-dependent extra draw -- which would remap seeds -- is caught.
    # Compare the call COUNT across a seed of each kind; comparing rng state would
    # not work, because a randint's underlying bit consumption varies with its value
    # (see the draw_workload doc).
    counts = []
    for kind in ("mesh", "cyclic", "backpressure", "iso", "actorref"):
        seed = next(s for s in range(0, 500)
                    if common.draw_workload(random.Random(s))[0] == kind)
        counter = _CallCountingRandom(seed)
        common.draw_workload(counter)
        counts.append(counter.calls)
    check("draw_workload makes the same number of rng calls for every kind",
          (len(set(counts)) == 1) and (counts[0] > 0))


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
        # SIGABRT must stay OUT of the pass-through list: the timeout hook
        # aborts the inferior and relies on lldb's default stop to print the
        # backtrace (see lldb_timeout_stop).
        check("lldb_argv (posix): SIGABRT is NOT passed through",
              "process handle SIGABRT" not in joined)


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


def test_posix_inferior_pid():
    # The Linux tree: lldb -> lldb-server -> inferior. The walk descends through
    # lldb's own machinery and returns the first process that isn't part of it.
    linux_tree = ["  100     1 lldb",
                  "  101   100 lldb-server",
                  "  102   101 generative",
                  "  200     1 unrelated"]
    check("posix_inferior_pid: walks through lldb-server to the inferior",
          common._posix_inferior_pid(100, linux_tree) == 102)
    # The macOS tree: lldb -> debugserver -> inferior, with ps giving comm as a
    # full path.
    mac_tree = ["  100     1 /usr/bin/lldb",
                "  101   100 /Library/.../debugserver",
                "  102   101 /private/tmp/generative"]
    check("posix_inferior_pid: walks through debugserver (macOS, full paths)",
          common._posix_inferior_pid(100, mac_tree) == 102)
    # No inferior (it exited, or lldb never launched it) -> None; the timeout
    # hook then falls back to the plain kill.
    check("posix_inferior_pid: no inferior -> None",
          common._posix_inferior_pid(
              100, ["  100     1 lldb", "  101   100 lldb-server"]) is None)
    check("posix_inferior_pid: unknown root -> None",
          common._posix_inferior_pid(999, linux_tree) is None)
    # A cyclic snapshot must terminate (the seen set), not loop forever. Real
    # ps output can't cycle, but the walk must not hang the hang-handler if it
    # somehow does.
    check("posix_inferior_pid: cyclic snapshot terminates -> None",
          common._posix_inferior_pid(
              100, ["  100   101 lldb-server", "  101   100 lldb-server"])
          is None)
    # A failed or stalled ps snapshot degrades to None (the caller then falls
    # back to the plain tree kill) instead of crashing the orchestrator from
    # inside its own hang handler.
    real_run = common.subprocess.run

    def ps_gone(*args, **kwargs):
        raise OSError("ps not found")

    try:
        common.subprocess.run = ps_gone
        check("posix_inferior_pid: ps failure -> None",
              common._posix_inferior_pid(100) is None)
    finally:
        common.subprocess.run = real_run


def test_windows_inferior_pid():
    # Every case that supplies `rows` MUST also supply `has_debugger`: left at its
    # default the walk calls the real Win32, which on POSIX raises inside
    # _windows_has_debugger's AttributeError guard and so reports "no debugger" for
    # every process. The case would then pass on Windows and fail here, for a
    # reason that has nothing to do with what it is testing.
    #
    # The Windows tree: lldb debugs natively, so the inferior is a direct child --
    # but lldb also spawns a conhost.exe, and the inferior gets one of its own. The
    # walk must pick the process that has a debugger attached, NOT the first
    # non-lldb child, which is why the conhost is listed first here.
    windows_tree = [(100, 1), (101, 100), (102, 100), (103, 102), (200, 1)]
    debugged = {102}
    check("windows_inferior_pid: picks the debugged child, not the conhost",
          common._windows_inferior_pid(
              100, windows_tree, lambda pid: pid in debugged) == 102)
    # A debugged grandchild is still found: the walk descends past children that
    # are not the inferior rather than stopping at the first layer.
    check("windows_inferior_pid: descends past a non-debugged child",
          common._windows_inferior_pid(
              100, windows_tree, lambda pid: pid == 103) == 103)
    # No inferior (it exited, or lldb never launched it) -> None; the timeout hook
    # then falls back to the plain kill.
    check("windows_inferior_pid: nothing debugged -> None",
          common._windows_inferior_pid(
              100, windows_tree, lambda pid: False) is None)
    # A debugged process outside lldb's subtree (another debugger's inferior) is
    # never returned: the walk starts from lldb, it does not scan every process.
    check("windows_inferior_pid: a debugged process outside the subtree is ignored",
          common._windows_inferior_pid(
              100, windows_tree, lambda pid: pid == 200) is None)
    check("windows_inferior_pid: unknown root -> None",
          common._windows_inferior_pid(
              999, windows_tree, lambda pid: pid in debugged) is None)
    # A cyclic snapshot must terminate (the seen set), not loop forever. Real
    # Toolhelp32 output can't cycle, but the walk must not hang the hang-handler
    # if it somehow does.
    check("windows_inferior_pid: cyclic snapshot terminates -> None",
          common._windows_inferior_pid(
              100, [(100, 101), (101, 100)], lambda pid: False) is None)
    # A failed process snapshot degrades to None (the caller then falls back to the
    # plain tree kill) instead of crashing the orchestrator from inside its own
    # hang handler. Patching the snapshot -- rather than passing rows=None, which
    # means "take a real one" -- is what lets this case run on any host.
    real_rows = common._windows_process_rows
    try:
        common._windows_process_rows = lambda: None
        check("windows_inferior_pid: snapshot failure -> None",
              common._windows_inferior_pid(100) is None)
    finally:
        common._windows_process_rows = real_rows


def test_lldb_timeout_stop():
    # Every arm patches os.name, so each platform's dispatch is exercised on every
    # host -- the Windows arm on the Linux CI job, the POSIX arm on the Windows
    # one. Patching os.name is safe here: lldb_timeout_stop reads it directly, and
    # nothing else runs concurrently in this suite. (os.kill, signal.SIGABRT,
    # ProcessLookupError and PermissionError all exist on Windows Python, and the
    # POSIX arm fakes os.kill anyway, so it never signals anything real.)
    real_name = os.name
    real_kill = os.kill
    real_find = common._posix_inferior_pid
    sent = []
    proc = types.SimpleNamespace(pid=100)
    try:
        os.name = "posix"
        os.kill = lambda pid, sig: sent.append((pid, sig))
        common._posix_inferior_pid = lambda lldb_pid: 4242
        check("lldb_timeout_stop (posix): aborts the found inferior pid",
              common.lldb_timeout_stop(proc) is True
              and sent == [(4242, common.signal.SIGABRT)])
        sent.clear()
        common._posix_inferior_pid = lambda lldb_pid: None
        with _captured_info() as logs:
            no_inferior = common.lldb_timeout_stop(proc)
        check("lldb_timeout_stop (posix): no inferior -> False, nothing signaled",
              no_inferior is False and sent == [])
        # Says WHY there is no backtrace. The grace-expiry path says something
        # different (see test_watch_for_progress_timeout_hook); an empty bundle is
        # only actionable if the log distinguishes them.
        check("lldb_timeout_stop (posix): no inferior is reported as such",
              any("no lldb inferior" in line for line in logs.lines))

        def gone(pid, sig):
            raise ProcessLookupError()

        os.kill = gone
        common._posix_inferior_pid = lambda lldb_pid: 4242
        with _captured_info() as logs:
            already_gone = common.lldb_timeout_stop(proc)
        check("lldb_timeout_stop (posix): inferior already gone -> False",
              already_gone is False)
        check("lldb_timeout_stop (posix): a failed abort is reported as such",
              any("could not abort" in line for line in logs.lines))

        def refused(pid, sig):
            raise PermissionError()

        os.kill = refused
        check("lldb_timeout_stop (posix): kill not permitted -> False",
              common.lldb_timeout_stop(proc) is False)
    finally:
        os.name = real_name
        os.kill = real_kill
        common._posix_inferior_pid = real_find

    # Windows: no SIGABRT to send, so the hook injects a breakpoint instead.
    real_win_find = common._windows_inferior_pid
    real_break = common._windows_debug_break
    broke = []
    try:
        os.name = "nt"
        common._windows_debug_break = lambda pid: broke.append(pid) or True
        common._windows_inferior_pid = lambda lldb_pid: 4242
        check("lldb_timeout_stop (windows): breaks into the found inferior pid",
              common.lldb_timeout_stop(proc) is True and broke == [4242])
        broke.clear()
        common._windows_inferior_pid = lambda lldb_pid: None
        check("lldb_timeout_stop (windows): no inferior -> False, nothing broken",
              common.lldb_timeout_stop(proc) is False and broke == [])
        common._windows_inferior_pid = lambda lldb_pid: 4242
        common._windows_debug_break = lambda pid: False
        with _captured_info() as logs:
            refused = common.lldb_timeout_stop(proc)
        check("lldb_timeout_stop (windows): break refused -> False",
              refused is False)
        check("lldb_timeout_stop (windows): a refused break is reported as such",
              any("could not break into" in line for line in logs.lines))
    finally:
        os.name = real_name
        common._windows_inferior_pid = real_win_find
        common._windows_debug_break = real_break

    # Neither POSIX nor Windows: the hook declines up front, walking nothing. The
    # caller falls back to the plain tree kill.
    walked = []
    try:
        os.name = "java"
        common._posix_inferior_pid = lambda lldb_pid: walked.append(lldb_pid)
        common._windows_inferior_pid = lambda lldb_pid: walked.append(lldb_pid)
        check("lldb_timeout_stop: unknown platform -> False without walking",
              common.lldb_timeout_stop(proc) is False and walked == [])
    finally:
        os.name = real_name
        common._posix_inferior_pid = real_find
        common._windows_inferior_pid = real_win_find


class _FakeKernel32:
    """A stand-in for the real kernel32, so the Win32 wrappers' FAILURE paths can
    be driven from any host. Their happy path is covered on Windows by the
    real-lldb integration test; these paths -- a refused snapshot, a refused
    OpenProcess, a target that stopped being debugged -- never occur there, and a
    regression in one of them silently costs a backtrace.

    The out-params are filled through `ctypes.byref(x)._obj`, which is the object
    the reference was taken of, so the wrappers see real writes."""

    def __init__(self, snapshot=1, rows=(), open_handle=7, debugger=True,
                 check_ok=True, break_ok=True):
        self.snapshot = snapshot
        self.rows = list(rows)
        self.open_handle = open_handle
        self.debugger = debugger
        self.check_ok = check_ok
        self.break_ok = break_ok
        self.opened = []
        self.broke = []
        self.closed = []
        self._cursor = 0

    def CreateToolhelp32Snapshot(self, flags, pid):
        return self.snapshot

    def _fill(self, ref):
        if self._cursor >= len(self.rows):
            return 0
        pid, ppid = self.rows[self._cursor]
        self._cursor += 1
        ref._obj.th32ProcessID = pid
        ref._obj.th32ParentProcessID = ppid
        return 1

    def Process32First(self, snapshot, ref):
        self._cursor = 0
        return self._fill(ref)

    def Process32Next(self, snapshot, ref):
        return self._fill(ref)

    def OpenProcess(self, access, inherit, pid):
        self.opened.append((access, pid))
        return self.open_handle

    def CheckRemoteDebuggerPresent(self, handle, ref):
        if not self.check_ok:
            return 0
        ref._obj.value = 1 if self.debugger else 0
        return 1

    def DebugBreakProcess(self, handle):
        self.broke.append(handle)
        return 1 if self.break_ok else 0

    def CloseHandle(self, handle):
        self.closed.append(handle)


def _with_kernel32(fake, body):
    real = common._windows_kernel32
    try:
        common._windows_kernel32 = lambda: fake
        return body()
    finally:
        common._windows_kernel32 = real


def test_windows_win32_wrappers():
    # A refused snapshot degrades to None, NOT to an empty process list: an empty
    # list would say "lldb has no descendants" and the caller would kill without a
    # backtrace for the wrong reason. CreateToolhelp32Snapshot signals failure with
    # INVALID_HANDLE_VALUE, not NULL, so both are checked.
    fake = _FakeKernel32(snapshot=common._INVALID_HANDLE_VALUE)
    check("windows_process_rows: INVALID_HANDLE_VALUE snapshot -> None",
          _with_kernel32(fake, common._windows_process_rows) is None)
    fake = _FakeKernel32(snapshot=0)
    check("windows_process_rows: NULL snapshot -> None",
          _with_kernel32(fake, common._windows_process_rows) is None)
    fake = _FakeKernel32(snapshot=5, rows=[(100, 1), (101, 100)])
    check("windows_process_rows: walks the snapshot and closes it",
          _with_kernel32(fake, common._windows_process_rows)
          == [(100, 1), (101, 100)] and fake.closed == [5])

    # A process that exited between the snapshot and the query is not the inferior.
    fake = _FakeKernel32(open_handle=0)
    check("windows_has_debugger: OpenProcess refused -> False",
          _with_kernel32(fake, lambda: common._windows_has_debugger(4242))
          is False)
    fake = _FakeKernel32(check_ok=False)
    check("windows_has_debugger: CheckRemoteDebuggerPresent failed -> False",
          _with_kernel32(fake, lambda: common._windows_has_debugger(4242))
          is False and fake.closed == [7])
    fake = _FakeKernel32(debugger=False)
    check("windows_has_debugger: no debugger attached -> False",
          _with_kernel32(fake, lambda: common._windows_has_debugger(4242))
          is False)
    fake = _FakeKernel32(debugger=True)
    check("windows_has_debugger: debugger attached -> True, handle closed",
          _with_kernel32(fake, lambda: common._windows_has_debugger(4242))
          is True and fake.closed == [7]
          and fake.opened == [(common._PROCESS_QUERY_INFORMATION, 4242)])

    # The load-bearing guard: an int3 injected into a process with NO debugger
    # attached crashes it. _windows_debug_break must re-check on the handle it
    # holds and refuse, or a recycled pid means we crash a bystander.
    fake = _FakeKernel32(debugger=False)
    with _captured_info() as logs:
        refused = _with_kernel32(fake, lambda: common._windows_debug_break(4242))
    check("windows_debug_break: target not debugged -> False, nothing injected",
          refused is False and fake.broke == [] and fake.closed == [7])
    check("windows_debug_break: refusing to break an undebugged pid is reported",
          any("no longer a debugged process" in line for line in logs.lines))
    fake = _FakeKernel32(debugger=True)
    check("windows_debug_break: debugged target -> injects, True, handle closed",
          _with_kernel32(fake, lambda: common._windows_debug_break(4242))
          is True and fake.broke == [7] and fake.closed == [7]
          and fake.opened == [(common._PROCESS_ALL_ACCESS, 4242)])
    fake = _FakeKernel32(open_handle=0)
    check("windows_debug_break: OpenProcess refused -> False",
          _with_kernel32(fake, lambda: common._windows_debug_break(4242))
          is False and fake.broke == [])
    fake = _FakeKernel32(debugger=True, break_ok=False)
    check("windows_debug_break: DebugBreakProcess refused -> False",
          _with_kernel32(fake, lambda: common._windows_debug_break(4242))
          is False)

    # On POSIX the real _windows_kernel32 raises (no ctypes.WinDLL). Every wrapper
    # must degrade rather than let that escape into the hang handler.
    def raises():
        raise AttributeError("no WinDLL on this platform")

    real = common._windows_kernel32
    try:
        common._windows_kernel32 = raises
        check("windows wrappers: a missing WinDLL degrades, never raises",
              common._windows_process_rows() is None
              and common._windows_has_debugger(1) is False
              and common._windows_debug_break(1) is False)
    finally:
        common._windows_kernel32 = real


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
        # A Windows crash words its stop as an Exception, not a signal, so the
        # signal parse never fires. It must still land on `fail` -- via the absent
        # exit-status line -- with its backtrace already in the captured output.
        # Real text, from an access violation under the pinned lldb.
        common._capture = _fake_capture(
            False, 1, "(lldb) run\n* thread #1, stop reason = Exception "
            "0xc0000005 encountered at address 0x7ff600f97260: Access violation "
            "writing location 0x00000000\n"
            "    frame #0: 0x00007ff600f97260 crasher.exe`main at crasher.c:2\n"
            "(lldb) quit 1\n")
        r = common.run_under_lldb("/bin/x", _MIN_CONFIG, "lldb", 60, None)
        check("run_under_lldb: a Windows crash (Exception, not signal) -> fail",
              r.outcome == "fail" and r.signal == "crash"
              and "frame #" in r.stdout)
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
        # A timeout whose output carries the hook's backtrace (including its
        # "stop reason = signal SIGABRT" stop line) is STILL a timeout -- the
        # timed_out short-circuit runs before the crash classification -- and
        # the backtrace rides into the bundle through the returned stdout.
        common._capture = _fake_capture(
            True, None,
            "Process 99 launched\n* stop reason = signal SIGABRT\n"
            "thread #1\n  frame #0: park_site\n")
        r = common.run_under_lldb("/bin/x", _MIN_CONFIG, "lldb", 60, None)
        check("run_under_lldb: timeout with a captured backtrace stays a timeout",
              r.outcome == "timeout" and r.signal is None
              and "frame #0: park_site" in r.stdout)

        # The lldb runner must arm the timeout hook -- without it a timeout
        # kills the run with no backtrace.
        seen_kwargs = []

        def recording_capture(*args, **kwargs):
            seen_kwargs.append(kwargs)
            return (False, 0, "Process 7 exited with status = 0 (0x0)", "")

        common._capture = recording_capture
        common.run_under_lldb("/bin/x", _MIN_CONFIG, "lldb", 60, None)
        check("run_under_lldb: arms lldb_timeout_stop as the timeout hook",
              seen_kwargs[0].get("on_timeout") is common.lldb_timeout_stop)
    finally:
        common._capture = real


def test_watchdog_should_kill():
    # The pure no-progress decision: kill on a silence gap OR the absolute backstop;
    # spare a run that keeps producing output, however long it legitimately takes.
    decide = common._watchdog_should_kill
    check("watchdog kills on a no-progress gap (a hang)",
          decide(400, 0, 0, 6000, 300))
    check("watchdog spares a slow-but-progressing run",
          not decide(5000, 0, 4990, 6000, 300))
    check("watchdog kills at the absolute backstop despite recent progress",
          decide(6001, 0, 6000, 6000, 300))
    # no_progress_seconds=None is the systematic flat mode: silence alone never
    # kills; only the total wall-clock timeout does.
    check("watchdog (flat mode): silence inside the timeout is spared",
          not decide(100, 0, 0, 180, None))
    check("watchdog (flat mode): kills past the flat timeout",
          decide(181, 0, 181, 180, None))


class _FakeStream:
    def __init__(self, lines):
        self._lines = list(lines)

    def readline(self):
        return self._lines.pop(0) if self._lines else b""

    def close(self):
        pass


class _captured_info:
    """Collect what the code under test logs through common.info, so the messages
    that tell two otherwise-identical failure paths apart can be asserted."""

    def __init__(self):
        self.lines = []

    def __enter__(self):
        self._real = common.info
        common.info = self.lines.append
        return self

    def __exit__(self, *exc):
        common.info = self._real
        return False


class _FakeProc:
    """A subprocess stand-in for _watch_for_progress: poll() returns None while
    `alive_polls` remain (the run is going), then the exit code."""
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
    real_kill = common._kill_process_tree
    killed = []
    common._kill_process_tree = lambda p: killed.append(p)
    try:
        # Clean completion: the process exits, both streams are drained, the exit
        # code passes through, and nothing is killed.
        proc = _FakeProc([b"HEARTBEAT\n", b"RECEIVED=1 SENT=1 EXPECTED=1\n"],
                         [b"(lldb) run\n"], alive_polls=0, returncode=0)
        timed_out, rc, out, err = common._watch_for_progress(
            proc, 6000, 300, poll=lambda: 0.0, sleep=lambda _s: None)
        check("watch_for_progress: clean run is not timed out", timed_out is False)
        check("watch_for_progress: exit code passes through", rc == 0)
        check("watch_for_progress: stdout is drained", "RECEIVED=1" in out)
        check("watch_for_progress: stderr is drained", "lldb" in err)
        check("watch_for_progress: a clean run is not killed", not killed)

        # Hang: the process stays alive and emits nothing -> killed on no-progress,
        # reported as timed out with no returncode. The injected sleep advances the
        # clock past the no-progress window without real waiting.
        clock = [0.0]

        def advance(_s):
            clock[0] += 1000.0

        proc2 = _FakeProc([], [], alive_polls=1000, returncode=0)
        timed_out2, rc2, _o, _e = common._watch_for_progress(
            proc2, 6000, 300, poll=lambda: clock[0], sleep=advance)
        check("watch_for_progress: a hang is timed out", timed_out2 is True)
        check("watch_for_progress: a hung run is killed", len(killed) == 1)
        check("watch_for_progress: a timed-out run has no returncode", rc2 is None)

        # Alive across several poll iterations, but the clock stays inside the
        # no-progress window each step -> the loop runs, the watchdog spares the run,
        # and it completes cleanly (not killed).
        clock3 = [0.0]

        def small_advance(_s):
            clock3[0] += 100.0  # < the 300s no-progress window per step

        proc3 = _FakeProc([b"HEARTBEAT\n"], [], alive_polls=2, returncode=0)
        timed_out3, rc3, _o3, _e3 = common._watch_for_progress(
            proc3, 6000, 300, poll=lambda: clock3[0], sleep=small_advance)
        check("watch_for_progress: an advancing run within the window completes",
              (timed_out3 is False) and (rc3 == 0))
        check("watch_for_progress: an advancing run is not killed", len(killed) == 1)

        # Flat mode (no_progress_seconds=None, the systematic watchdog): a
        # silent-but-alive run is spared until the total timeout, then killed.
        clock4 = [0.0]

        def advance4(_s):
            clock4[0] += 100.0

        proc4 = _FakeProc([], [], alive_polls=1000, returncode=0)
        timed_out4, rc4, _o4, _e4 = common._watch_for_progress(
            proc4, 180, None, poll=lambda: clock4[0], sleep=advance4)
        check("watch_for_progress (flat): killed at the flat timeout only",
              timed_out4 is True and rc4 is None and len(killed) == 2)
    finally:
        common._kill_process_tree = real_kill


def test_watch_for_progress_timeout_hook():
    real_kill = common._kill_process_tree
    killed = []
    common._kill_process_tree = lambda p: killed.append(p)
    try:
        # The injected sleep advances the fake clock AND yields briefly so the
        # reader threads drain the fake streams before the watchdog fires.
        def clocked(clock, step):
            def advance(_s):
                clock[0] += step
                time.sleep(0.01)
            return advance

        # Hook returns False (no inferior to abort): called exactly once with
        # the child process, then the tree is killed as it is without a hook.
        calls = []

        def refuse(p):
            calls.append(p)
            return False

        clock = [0.0]
        proc = _FakeProc([b"some output\n"], [], alive_polls=1000)
        timed_out, rc, _o, _e = common._watch_for_progress(
            proc, 180, None, poll=lambda: clock[0], sleep=clocked(clock, 1000.0),
            on_timeout=refuse)
        check("timeout hook: fires exactly once", len(calls) == 1)
        check("timeout hook: receives the child process", calls[0] is proc)
        check("timeout hook: False -> the tree is killed",
              timed_out is True and len(killed) == 1)

        # Hook returns True and the child exits during the grace (lldb printed
        # the backtrace and quit): NO tree kill. alive_polls=3 is load-bearing:
        # the watchdog loop consumes two polls and the post-hook liveness check
        # a third, so the exit is only reachable through the grace loop's own
        # polling -- removing the grace wait makes this proc still-alive at the
        # kill decision and the assertion fires.
        clock2 = [0.0]
        proc2 = _FakeProc([], [], alive_polls=3, returncode=1)
        timed_out2, rc2, _o2, _e2 = common._watch_for_progress(
            proc2, 180, None, poll=lambda: clock2[0],
            sleep=clocked(clock2, 1000.0), on_timeout=lambda p: True)
        check("timeout hook: graceful exit within the grace is not tree-killed",
              timed_out2 is True and rc2 is None and len(killed) == 1)

        # Hook returns True but the child never exits: tree-killed when the
        # grace expires. The kill discards everything lldb buffered, so the bundle
        # carries no backtrace -- indistinguishable, from the bundle alone, from a
        # run where no inferior was found. The log is what tells them apart (they
        # want different fixes), so assert it is emitted.
        clock3 = [0.0]
        proc3 = _FakeProc([], [], alive_polls=1000)
        logs3 = _captured_info()
        with logs3:
            timed_out3, _rc3, _o3, _e3 = common._watch_for_progress(
                proc3, 180, None, poll=lambda: clock3[0],
                sleep=clocked(clock3, 1000.0), on_timeout=lambda p: True)
        check("timeout hook: grace expiry falls back to the tree kill",
              timed_out3 is True and len(killed) == 2)
        check("timeout hook: grace expiry says so, so an empty bundle is "
              "attributable",
              any("grace" in line for line in logs3.lines))

        # A hook that RAISES must not escape: the run is already a failure, and an
        # exception here would abort the soak and orphan the debugger and the hung
        # engine instead of recording the timeout we already know about.
        clock5 = [0.0]
        proc5 = _FakeProc([], [], alive_polls=1000)
        logs5 = _captured_info()

        def explode(_p):
            raise RuntimeError("process table went away")

        with logs5:
            timed_out5, rc5, _o5, _e5 = common._watch_for_progress(
                proc5, 180, None, poll=lambda: clock5[0],
                sleep=clocked(clock5, 1000.0), on_timeout=explode)
        check("timeout hook: a raising hook degrades to the tree kill",
              timed_out5 is True and rc5 is None and len(killed) == 3)
        check("timeout hook: a raising hook is reported, not swallowed",
              any("RuntimeError" in line for line in logs5.lines))

        # The hook also fires in the normal mode's silence-triggered watchdog
        # (no_progress_seconds set, total timeout far away) -- the production
        # trigger for a real hang.
        calls4 = []
        clock4 = [0.0]
        proc4 = _FakeProc([], [], alive_polls=1000)
        timed_out4, _rc4, _o4, _e4 = common._watch_for_progress(
            proc4, 6000, 300, poll=lambda: clock4[0],
            sleep=clocked(clock4, 1000.0),
            on_timeout=lambda p: calls4.append(p) or False)
        # clock4 < 6000 pins that the SILENCE window (300) triggered the kill,
        # not the 6000 backstop -- without it a broken silence check would
        # still pass via the backstop firing the same hook later.
        check("timeout hook: fires on a silence-triggered (no-progress) kill",
              timed_out4 is True and len(calls4) == 1 and len(killed) == 4
              and clock4[0] < 6000)
    finally:
        common._kill_process_tree = real_kill


def test_capture_direct_flat_timeout():
    # The direct (no-lldb) path against a real subprocess: a flat timeout must
    # kill the child and report timed_out with no returncode. This is the
    # production path for --no-lldb (hosts without lldb; no CI lane passes it),
    # which lost its dedicated communicate() branch in the reader-thread
    # unification.
    if os.name != "posix":
        print("skip - direct flat-timeout (not POSIX)")
        return
    timed_out, rc, _out, _err = common._capture(
        [sys.executable, "-c", "import time; time.sleep(600)"], 1, None)
    check("direct flat timeout: real subprocess is killed and classified",
          timed_out is True and rc is None)


def test_lldb_timeout_capture_integration():
    # Real lldb, end to end: a hung inferior plus the timeout hook must produce
    # a thread backtrace in the captured output, with lldb exiting on its own
    # inside the grace (no tree kill). This is the standing guard for what fakes
    # cannot cover: the process-tree shape the inferior walk depends on
    # (lldb-server/debugserver on POSIX, a direct child alongside a conhost.exe on
    # Windows), lldb's default stop on the stop the hook induces, the on-crash
    # command chain, and lldb's flush-of-buffered-output at exit. On Windows it is
    # also the ONLY test that runs the real ctypes wrappers
    # (_windows_process_rows / _windows_has_debugger / _windows_debug_break).
    # (The sleeper's `main` does not resolve, so lldb_argv's signal-handle setup
    # is inert here -- the pass-through list is guarded textually in
    # test_lldb_argv, and its application to the real engine is proven by the
    # stress lanes themselves, where a non-applied SIGUSR2 pass-through would
    # hang every run.) The flat timeout is a GENEROUS 60s on purpose: the
    # sleeper never exits on its own, so the watchdog always fires mid-sleep as
    # long as the timeout exceeds lldb's launch time -- generous is what keeps
    # this deterministic on a slow runner. The no-tree-kill assertion rides the
    # 30s grace: lldb only has to backtrace one sleeping thread and quit, far
    # inside that margin even on a loaded machine.
    #
    # The stop marker differs by platform because the mechanism does: POSIX sends
    # SIGABRT, Windows injects a breakpoint (EXCEPTION_BREAKPOINT = 0x80000003).
    if os.name == "posix":
        stop_marker = "stop reason = signal SIGABRT"
    elif os.name == "nt":
        stop_marker = "stop reason = Exception 0x80000003"
    else:
        print("skip - lldb integration (neither POSIX nor Windows)")
        return
    # PONY_STRESS_LLDB names the lldb to use, the way the stress orchestrators
    # take --lldb. Windows needs it: the GitHub runner image ships an lldb at
    # C:\Program Files\LLVM\bin that exits immediately instead of running the
    # target, so `which` finds an lldb that cannot drive this test. Unset, this
    # falls back to PATH, which is what the Linux job wants.
    lldb = os.environ.get("PONY_STRESS_LLDB") or shutil.which("lldb")
    if lldb is None:
        print("skip - lldb integration (no lldb on PATH and no PONY_STRESS_LLDB; "
              "CI provides one)")
        return
    print("note: lldb integration is using %s" % lldb)
    sleeper = [sys.executable, "-c", "import time; time.sleep(600)"]
    real_kill = common._kill_process_tree
    tree_kills = []

    def record_kill(proc):
        tree_kills.append(proc)
        real_kill(proc)

    common._kill_process_tree = record_kill
    try:
        timed_out, rc, out, err = common._capture(
            common.lldb_argv(lldb, sleeper), 60, None,
            on_timeout=common.lldb_timeout_stop)
    finally:
        common._kill_process_tree = real_kill
    combined = out + "\n" + err
    check("lldb integration: the run timed out", timed_out is True)
    check("lldb integration: lldb stopped the inferior", stop_marker in combined)
    check("lldb integration: a thread backtrace was captured",
          "frame #" in combined)
    check("lldb integration: lldb exited within the grace (no tree kill)",
          not tree_kills)


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


def test_summary_line():
    # summary_line reads DIFFERENT shape fields per kind (mesh/cyclic have
    # chains/ttl, backpressure has producers/messages/apply-every, iso has
    # pingers/chains/ttl/node-size). If its read keys ever drift from
    # resolve_config's emit keys, it raises an unguarded KeyError -- at soak time,
    # never in CI -- so exercise every branch here. The shape dicts mirror what the
    # orchestrators emit (see orchestrate_normal.resolve_config and
    # draw_systematic_workload).
    result = common.RunResult(
        "pass", 0, None,
        "RECEIVED=10 SENT=10 EXPECTED=10 ORDER_SIG=42", "")

    mesh = {"master_seed": 1, "runtime": {},
            "workload": {"seed": 9, "workload": "mesh", "pingers": 8,
                         "chains": 8, "ttl": 16, "payload": "u64",
                         "payload-size": 1, "payload-mode": "forward"}}
    cyclic = {"master_seed": 2, "runtime": {},
              "workload": {"seed": 9, "workload": "cyclic", "generations": 50,
                           "group": 4, "chains": 2, "ttl": 8, "payload": "string",
                           "payload-size": 64, "payload-mode": "fresh"}}
    bp = {"master_seed": 3, "runtime": {},
          "workload": {"seed": 9, "workload": "backpressure", "producers": 64,
                       "messages": 1000, "apply-every": 200, "payload": "string",
                       "payload-size": 64, "payload-mode": "forward"}}
    iso = {"master_seed": 6, "runtime": {},
           "workload": {"seed": 9, "workload": "iso", "pingers": 8,
                        "chains": 100, "ttl": 8, "node-size": 64,
                        "node-depth": 3, "node-breadth": 2}}
    actorref = {"master_seed": 7, "runtime": {},
                "workload": {"seed": 9, "workload": "actorref", "pingers": 8,
                             "chains": 100, "ttl": 8}}
    # A config with NO `workload` key (older or hand-built) must default to the mesh
    # branch -- both orchestrators emit the key now, but the fallback still guards it.
    no_kind = {"master_seed": 4, "runtime": {},
               "workload": {"seed": 9, "pingers": 16, "chains": 100, "ttl": 4,
                            "payload": "u64", "payload-size": 8,
                            "payload-mode": "fresh"}}

    # Each kind must render without KeyError and name its kind + its own fields.
    ms = common.summary_line(mesh, result)
    check("summary_line mesh: names kind and pingers/chains",
          ("mesh pingers=8" in ms) and ("chains=8" in ms) and ("PASS" in ms))
    cs = common.summary_line(cyclic, result)
    check("summary_line cyclic: names generations/group/chains",
          ("cyclic generations=50 group=4" in cs) and ("chains=2" in cs))
    bs = common.summary_line(bp, result)
    check("summary_line backpressure: names producers/messages/apply_every",
          ("backpressure producers=64 messages=1000 apply_every=200" in bs)
          and ("chains" not in bs))   # backpressure has no chains/ttl
    isos = common.summary_line(iso, result)
    check("summary_line iso: names its cargo knobs + payload=none (not 'mesh')",
          ("iso pingers=8 chains=100 ttl=8 node_size=64 depth=3 breadth=2" in isos)
          and ("payload=none" in isos) and ("mesh" not in isos))
    ars = common.summary_line(actorref, result)
    check("summary_line actorref: names relays/chains/ttl + payload=none (not 'mesh')",
          ("actorref relays=8 chains=100 ttl=8" in ars)
          and ("payload=none" in ars) and ("mesh" not in ars))
    ss = common.summary_line(no_kind, result)
    check("summary_line defaults a no-workload-key config to mesh",
          "mesh pingers=16" in ss)
    # The detail suffix carries the parsed result on every kind.
    check("summary_line carries the parsed detail",
          "received=10 expected=10 sig=42" in bs)


def main():
    test_derive_seed()
    test_derive_seed_zero_floor()
    test_draw_systematic_workload_contract()
    test_draw_systematic_workload_coverage()
    test_draw_swarm_knobs()
    test_draw_payload_mode()
    test_draw_max_threads()
    test_draw_bucketed()
    test_draw_payload()
    test_clamp_ttl()
    test_draw_workload()
    test_build_argv()
    test_run_command()
    test_lldb_argv()
    test_lldb_exit_code()
    test_posix_inferior_pid()
    test_windows_inferior_pid()
    test_windows_win32_wrappers()
    test_lldb_timeout_stop()
    test_run_once()
    test_run_under_lldb()
    test_watchdog_should_kill()
    test_watch_for_progress()
    test_watch_for_progress_timeout_hook()
    test_capture_direct_flat_timeout()
    test_lldb_timeout_capture_integration()
    test_rlimit_as_supported()
    test_bundle_for()
    test_resolve_seeds()
    test_parse_result()
    test_summary_line()
    if FAILURES:
        print("%d failure(s): %s" % (len(FAILURES), ", ".join(FAILURES)))
        sys.exit(1)
    print("all stress_common_test checks passed")


if __name__ == "__main__":
    main()
