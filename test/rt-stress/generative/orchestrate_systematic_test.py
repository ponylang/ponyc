#!/usr/bin/env python3
"""Unit tests for orchestrate_systematic.py's resolve_config (the systematic-mode
config draw). Self-contained (no pytest): `python3 ..._test.py`, exits 0/1.

The goldens below pin resolve_config's exact output for one seed of each kind. Adding
the actorref kind to the mesh|cyclic|backpressure|iso set (reweighting the kind roll,
though actorref draws no new cargo) intentionally REMAPPED which seed rolls which
kind, so these are the current values; they guard that the seed->config mapping stays
stable from here -- any later reorder/narrow/added draw changes them and breaks
historical --replay.
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
            "ponymaxthreads": 5,
            "ponynoblock": True,
            "ponynoscale": True,
            "ponysystematictestingseed": 6974751086576699578,
        },
        "workload": {
            "chains": 184,
            "node-breadth": 1,
            "node-depth": 3,
            "node-size": 256,
            "pingers": 64,
            "seed": 2686188150644990173,
            "ttl": 7,
            "workload": "iso",
        },
    }
    check("resolve_config(0, 8) matches the pinned golden config",
          systematic.resolve_config(0, THREADS) == expected)


def test_resolve_config_cyclic_golden():
    # Seed 0 rolls iso, so its golden pins none of generations/group/payload/
    # payload-size (iso discards them). Pin a seed that rolls CYCLIC so those drawn
    # values -- and payload-mode's presence on a non-iso kind -- are also guarded.
    # Seed 9 is the first seed that rolls CYCLIC under the 4-way weights.
    expected = {
        "master_seed": 9,
        "runtime": {
            "ponycdinterval": 10,
            "ponygcfactor": 1.5,
            "ponygcinitial": 20,
            "ponymaxthreads": 7,
            "ponynoblock": True,
            "ponysystematictestingseed": 6795400069283718112,
        },
        "workload": {
            "chains": 2,
            "generations": 18,
            "group": 4,
            "payload": "string",
            "payload-mode": "forward",
            "payload-size": 4096,
            "seed": 7931586630182256735,
            "ttl": 11,
            "workload": "cyclic",
        },
    }
    check("resolve_config(9, 8) matches the pinned cyclic golden config",
          systematic.resolve_config(9, THREADS) == expected)


def test_resolve_config_backpressure_golden():
    # Pin a seed that rolls BACKPRESSURE so its drawn producers/messages/apply-every
    # -- and payload-mode's presence on this non-iso kind -- are guarded, and no
    # chains/ttl/pingers leak in. Seed 5 rolls backpressure under the 4-way weights.
    expected = {
        "master_seed": 5,
        "runtime": {
            "ponygcfactor": 1.5,
            "ponymaxthreads": 3,
            "ponynoblock": True,
            "ponynoscale": True,
            "ponysystematictestingseed": 1996155286158128287,
        },
        "workload": {
            "apply-every": 1,
            "messages": 128,
            "payload": "u64",
            "payload-mode": "fresh",
            "payload-size": 1024,
            "producers": 64,
            "seed": 1674455568713221789,
            "workload": "backpressure",
        },
    }
    check("resolve_config(5, 8) matches the pinned backpressure golden config",
          systematic.resolve_config(5, THREADS) == expected)


def test_resolve_config_actorref_golden():
    # Pin a seed that rolls ACTORREF: pingers/chains/ttl and NO val payload and NO
    # payload-mode (it draws-then-ignores both, like iso) and no other kind's fields.
    # Seed 2 rolls actorref; chains/ttl are the small systematic range (ttl >= 1).
    expected = {
        "master_seed": 2,
        "runtime": {
            "ponycdinterval": 100,
            "ponymaxthreads": 1,
            "ponysystematictestingseed": 3815088957855747605,
        },
        "workload": {
            "chains": 109,
            "pingers": 64,
            "seed": 10172212549529217571,
            "ttl": 2,
            "workload": "actorref",
        },
    }
    check("resolve_config(2, 8) matches the pinned actorref golden config",
          systematic.resolve_config(2, THREADS) == expected)


def test_resolve_config_deterministic_and_bounded():
    check("resolve_config is deterministic",
          systematic.resolve_config(7, THREADS)
          == systematic.resolve_config(7, THREADS))

    invariants_hold = True
    maxthreads_seen = set()
    kinds_seen = set()
    noblock_present = noblock_absent = 0
    pmode_ok = True
    pmode_values = set()
    for master in range(0, 300):
        config = systematic.resolve_config(master, THREADS)
        runtime = config["runtime"]
        workload = config["workload"]
        # ponynoblock is a swarm knob now (drawn ~50%): present-as-True or absent,
        # never a False value. The systematic seed is always set >= 1.
        nb = runtime.get("ponynoblock")
        if nb is True:
            noblock_present += 1
        elif nb is None:
            noblock_absent += 1
        else:
            invariants_hold = False
        if runtime["ponysystematictestingseed"] < 1:
            invariants_hold = False
        mt = runtime.get("ponymaxthreads")
        if (mt is None) or not (1 <= mt <= THREADS):
            invariants_hold = False
        else:
            maxthreads_seen.add(mt)
        kind = workload.get("workload")
        if kind not in ("mesh", "cyclic", "backpressure", "iso", "actorref"):
            invariants_hold = False
        kinds_seen.add(kind)
        # payload-mode is emitted for the val-payload kinds (mesh/cyclic/backpressure)
        # and OMITTED for iso/actorref (they draw-then-ignore it) -- a silent drop would
        # lose all fresh-mode coverage for the val kinds while conservation + determinism
        # pass.
        if kind in ("iso", "actorref"):
            if "payload-mode" in workload:
                pmode_ok = False
        elif "payload-mode" not in workload:
            pmode_ok = False
        else:
            pmode_values.add(workload["payload-mode"])
    check("systematic invariants hold over 300 seeds", invariants_hold)
    check("ponymaxthreads spans the full [1, THREADS] range",
          maxthreads_seen == set(range(1, THREADS + 1)))
    check("resolve_config draws all of mesh, cyclic, backpressure, iso, actorref",
          kinds_seen == {"mesh", "cyclic", "backpressure", "iso", "actorref"})
    check("ponynoblock is drawn both present and absent",
          (noblock_present > 0) and (noblock_absent > 0))
    check("payload-mode emitted for mesh/cyclic/backpressure, omitted for iso/actorref",
          pmode_ok)
    check("payload-mode covers {forward, fresh} across the val-payload kinds",
          pmode_values == {"forward", "fresh"})


def test_resolve_config_swarm_omission():
    # Each optional knob must appear in some runs and be absent in others.
    # ponynoblock is now a swarm knob too (drawn ~50%); ponymaxthreads and the
    # systematic seed are always set, so not omittable.
    omittable = ["ponynoblock", "ponynoscale", "ponygcinitial", "ponygcfactor",
                 "ponycdinterval"]
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
    test_resolve_config_cyclic_golden()
    test_resolve_config_backpressure_golden()
    test_resolve_config_actorref_golden()
    test_resolve_config_deterministic_and_bounded()
    test_resolve_config_swarm_omission()
    if FAILURES:
        print("%d failure(s): %s" % (len(FAILURES), ", ".join(FAILURES)))
        sys.exit(1)
    print("all orchestrate_systematic_test checks passed")


if __name__ == "__main__":
    main()
