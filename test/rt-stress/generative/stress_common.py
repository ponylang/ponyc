#!/usr/bin/env python3
"""Shared, mode-agnostic mechanism for the generative stress orchestrators.

Both orchestrators drive the same engine (main.pony) and share everything here:
seed derivation, the workload draws, the command line, the run watchdog, result
parsing, and the failure bundle.

  - orchestrate_systematic.py -- serialized, reproducible: runs the engine under
    `use=systematic_testing`, exploring scheduler interleavings deterministically.
  - orchestrate_normal.py -- the real, multi-threaded runtime: true parallelism,
    non-reproducible, catching the simultaneity bugs the systematic mode can't.

What DIFFERS between the two modes lives in the two drivers, NOT behind a flag in
here. This file holds only what is byte-identical to both, so neither mode reads
an `if systematic` branch in shared code.

Cross-platform: this runs on Linux, macOS, AND Windows. Both modes target all
three TIER1 platforms (on Windows the systematic build uses
`use=systematic_testing` alone -- Windows scales the scheduler with native
primitives, not pthreads). The RLIMIT_AS memory cap is applied only where the OS
honors it (Linux): Windows lacks the `resource` module and macOS rejects the
limit, so both fall back to host OOM handling. Both watchdogs are portable -- the
systematic mode's flat wall-clock timeout and the normal mode's no-progress
watchdog (threads + blocking reads, no `select`, so it runs on Windows too).

The pure pieces (derive_seed / draw_systematic_workload / draw_* / build_argv /
run_command / parse_result / lldb_argv / lldb_exit_code) are unit-tested in
stress_common_test.py; the run classifiers (run_once / run_under_lldb) are tested
by injecting a fake _capture.
"""
import hashlib
import json
import os
import re
import shlex
import signal
import subprocess
import sys
import threading
import time

try:
    import resource  # POSIX only; absent on Windows
except ImportError:
    resource = None

# The program name passed to `ponyc -b`, and the source package the orchestrators
# sit in (they compile their own directory).
BINARY_NAME = "generative"
SOURCE_DIR = os.path.dirname(os.path.abspath(__file__))
# Repo root is four levels up: test/rt-stress/generative/<script>.py. If these
# scripts move to a different depth, update the count (stress_common_test.py pins
# that packages/ resolves from here).
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(SOURCE_DIR)))

U64_MASK = (1 << 64) - 1
DEFAULT_TIMEOUT_SECONDS = 120
# Normal mode kills a run on NO PROGRESS, not on total length: the engine emits a
# flushed heartbeat as it advances, and a run that stays silent this long is hung
# (a deadlock, a lost message, or a shutdown that never completes). A slow-but-
# advancing run keeps heartbeating, so it is NOT killed -- it finishes, however
# long it legitimately takes. ~5 min is far above the worst inter-heartbeat gap
# (the engine targets ~tens of heartbeats per emitter over a run) yet catches a
# hang in minutes. COUPLING: paired with the engine's heartbeat cadence -- see
# .known-couplings/stress-heartbeat-watchdog.md.
DEFAULT_NORMAL_NO_PROGRESS_SECONDS = 300
# Absolute backstop for the normal mode: even a steadily-progressing run is capped
# here, bounding a config whose real time runs far past its estimate (a cost-model
# miss) so a single run can't eat the whole CI job. The magnitude draw (clamp_ttl)
# targets ~20 min, so this ~100 min leaves wide margin and rarely fires.
DEFAULT_NORMAL_TIMEOUT_SECONDS = 6000
DEFAULT_MEM_LIMIT_MB = 4096


def info(message):
    print(message, flush=True)


def die(message):
    print("FATAL: " + message, file=sys.stderr, flush=True)
    sys.exit(1)


def derive_seed(master_seed, label):
    """Deterministic, stdlib-only U64 seed from (master_seed, label), floored to
    >= 1. The floor is load-bearing: the runtime rejects systematic seed 0, and
    the engine's `Rand(seed, 0)` hits the degenerate all-zero xoroshiro state at
    0. Different labels yield independent seeds from the same master."""
    digest = hashlib.sha256(("%d:%s" % (master_seed, label)).encode()).digest()
    return (int.from_bytes(digest[:8], "big") & U64_MASK) or 1


def draw_swarm_knobs(rng, runtime):
    """Draw the four optional runtime knobs shared by both modes, adding the
    present ones to `runtime`. Omission IS the swarm mechanism -- each knob is
    independently present or absent. Draw order is part of the seed-stability
    contract; do not reorder.

    `ponynoblock` is deliberately NOT here: each driver draws it itself, right
    before this call (both modes now draw it ~50% as a swarm knob -- systematic no
    longer forces it, since the determinism oracle holds with the cycle detector
    on), so only these four knobs are common.
    """
    if rng.random() < 0.5:
        runtime["ponynoscale"] = True
    if rng.random() < 0.5:
        # --ponygcinitial is a base-2 EXPONENT, not a byte count: it defers an
        # actor's GC until it is using 2^N bytes (runtime default N=14 = 16 KiB).
        # 0 is a deliberate stress point -- a 1-byte threshold that forces GC on
        # nearly every allocation; 10/16/20 are 1 KiB / 64 KiB / 1 MiB, a spread
        # around the default. Pass exponents, NOT byte counts -- the runtime
        # computes `1 << N`, so a byte count like 65536 is masked (mod 64) to a
        # 1-byte threshold on x86-64.
        runtime["ponygcinitial"] = rng.choice([0, 10, 16, 20])
    if rng.random() < 0.5:
        runtime["ponygcfactor"] = round(rng.choice([1.05, 1.5, 2.0, 4.0]), 2)
    if rng.random() < 0.5:
        runtime["ponycdinterval"] = rng.choice([10, 100, 1000])


def draw_payload_mode(rng):
    """`forward` re-sends one payload object down a chain (shared-val
    refcounting); `fresh` allocates a new one at each hop (allocation + collection
    churn). Used only by the SYSTEMATIC driver now -- normal mode draws kind, size,
    and mode together in draw_payload. COUPLING: in the systematic driver this
    must be drawn AFTER the runtime-knob draws and BEFORE draw_max_threads() -- that
    position is its seed-stability contract; reordering it remaps every systematic
    seed."""
    return rng.choice(["forward", "fresh"])


def draw_max_threads(rng, max_threads):
    """`--ponymaxthreads`, drawn LAST and the only host-dependent field, across
    [1, max_threads] so the swarm spans serial (1) through all physical cores. It
    MUST stay last: randint() consumes a variable number of rng steps (its width
    depends on max_threads), so any draw after it would remap across hosts with
    different core counts. The runtime REJECTS --ponymaxthreads > physical cores,
    so max_threads is the probed physical count (see probe_max_threads)."""
    return rng.randint(1, max_threads)


# Normal-mode magnitude buckets. Each of chains and ttl draws a bucket
# (small/medium/large at 25/50/25), then a uniform value within it. Ranges are
# (lo, hi) inclusive. These set the magnitude RANGE only; run TIME is governed
# separately by clamp_ttl (below), which trims ttl so a config's estimated
# single-thread time stays within RUN_TIME_CEILING_SECONDS. The two are split
# because per-message cost is NOT flat: a forward-mode run's cost grows with
# chains^2 (the shared payload's per-hop trace cost scales with the number of
# concurrent live chains), so a high-chains forward run at high ttl would take
# hours -- the clamp pairs high chains with a low ttl (and vice versa), keeping
# both dimensions covered and dropping only the can't-finish corner. chains and
# ttl share these ranges. Systematic mode does NOT use them -- it stays
# small/fixed-range (see draw_systematic_workload). Locked, not provisional.
NORMAL_SIZE_BUCKETS = {
    "small": (1500, 14000),
    "medium": (14001, 27000),
    "large": (27001, 34000),
}

# String-payload byte sizes. Drawn freely; run time is governed by clamp_ttl, not
# by restricting which sizes a big run may pick.
STRING_SIZES = [1, 8, 64, 256, 1024, 4096]

# Per-run time ceiling (estimated single-thread seconds on the slow path). The
# largest mesh run targets ~this; smaller draws finish faster. ~20 min keeps the
# soak getting through many configs while no single run dominates the window, and
# matches the original "largest run ~15-20 min" intent. Tunable.
RUN_TIME_CEILING_SECONDS = 1200

# Mesh run-time cost model: estimated SINGLE-THREAD seconds on the slow CI path,
# measured on the engine (debug, under lldb on a 2-core runner is the worst TIER1
# path). Two shapes:
#   forward: time ~ K_fwd[payload] * chains^2 * (ttl+1). The one payload object is
#     forwarded the whole chain, so each hop re-traces it across an actor boundary;
#     that per-hop trace cost scales with the number of concurrent live chains, so
#     cost is quadratic in chains. It depends on payload KIND (a String traces more
#     than a boxed U64) but ~not its byte size (forwarding doesn't copy the bytes).
#   fresh:   time ~ K_fresh[payload][size] * chains * (ttl+1). A new payload is
#     allocated every hop, so cost is linear in chains and grows with the string's
#     byte size (allocation + tracing of the bytes).
# Constants are seconds-per-(unit of the shape above) and fold in a ~2x slow-CI
# margin over the local measurement. Bounding for 1 thread is deliberately the
# worst case: a multi-thread run finishes faster, so more seeds run -> more
# coverage. COUPLING: these are a property of the engine's per-hop cost -- re-measure
# (test/rt-stress/generative, sweep chains for each mode/payload/size) if the payload
# handling or the forwarding/trace path changes.
_MESH_FWD_COST = {"u64": 1.5e-9, "string": 6.0e-9}
_MESH_FRESH_COST_U64 = 4.0e-6
_MESH_FRESH_COST_STRING = {1: 8e-6, 8: 8e-6, 64: 9e-6, 256: 10e-6,
                           1024: 14e-6, 4096: 22e-6}


def est_mesh_seconds(chains, ttl, mode, payload, size):
    """Estimated single-thread slow-path run time (seconds) for a mesh config.
    See the cost-model comment above for the two shapes."""
    hops = chains * (ttl + 1)
    if mode == "forward":
        return _MESH_FWD_COST[payload] * chains * hops
    per_msg = (_MESH_FRESH_COST_U64 if payload == "u64"
               else _MESH_FRESH_COST_STRING[size])
    return per_msg * hops


def clamp_ttl(chains, ttl, mode, payload, size):
    """Trim ttl so a mesh config's estimated run time stays within
    RUN_TIME_CEILING_SECONDS, returning the (possibly unchanged) ttl. The estimate
    is linear in (ttl+1), so the per-(ttl+1) coefficient is est at ttl=0; the
    largest ttl that fits is ceiling/coefficient - 1, floored at 0. High chains in
    forward mode -> low ttl_max; low chains -> ttl unchanged. The 0 floor stays
    within the ceiling only while a single hop (the coefficient) does -- true for
    every drawable config (the max coefficient is ~7s, far under the ceiling); a
    bucket bump that broke that would fail test_clamp_ttl's max-chains case.
    Consumes no rng (a post-draw clamp), so only an over-budget ttl VALUE changes
    -- no draw remap."""
    if est_mesh_seconds(chains, ttl, mode, payload, size) <= RUN_TIME_CEILING_SECONDS:
        return ttl
    coefficient = est_mesh_seconds(chains, 0, mode, payload, size)
    ttl_max = int(RUN_TIME_CEILING_SECONDS / coefficient) - 1
    return max(0, min(ttl, ttl_max))

# Normal-mode actor counts. pingers=1 -- a lone actor that only ever forwards to
# itself -- is included on purpose: it exercises the ORCA self-send path. That path
# used to flood the forwarded object's owner with acquire messages and balloon memory
# (~4.5 GB at ~7.7M messages), so this draw once excluded pingers=1. But that was a
# real runtime defect, fixed in PR #5594: the in-queue self-sent object is pinned so
# the per-actor sweep no longer releases and re-borrows it every hop. With the fix a
# pingers=1 run is bounded (~40 MB even at the 34000-chain max, conserving), so
# drawing it here keeps a standing stress on that fix -- a regression would reappear
# as the same blowup.
NORMAL_PINGERS = [1, 2, 4, 8, 16, 32, 64]

# Normal-mode workload-kind weights for the 5-way draw (draw_workload). mesh is the
# baseline (the widest runtime surface); cyclic, backpressure, iso, and actorref each
# target one otherwise-untouched subsystem (the cycle detector; the explicit
# UNDER_PRESSURE muting path; ORCA's transfer of a nested mutable subgraph between
# actors; ORCA's actor-reference acquire/release of tags passed as message cargo). A
# run rolls mesh below MESH_P, cyclic below MESH_P + CYCLIC_P, backpressure below
# MESH_P + CYCLIC_P + BP_P, iso below MESH_P + CYCLIC_P + BP_P + ISO_P, else actorref.
# mesh keeps the largest share; the four targeted kinds split the rest evenly. The
# tests assert all five appear over the sampled seed range.
WORKLOAD_MESH_P = 0.4
WORKLOAD_CYCLIC_P = 0.15
WORKLOAD_BP_P = 0.15
WORKLOAD_ISO_P = 0.15   # actorref gets the remaining 1 - the four above

# Normal-mode cyclic-garbage draws (the `cyclic` workload). The cyclic workload
# spawns `generations` successive groups of `group` actors, each group a strongly
# connected reference cycle the detector must reclaim.
#
# Memory bound (calibrated on the engine, CD-OFF worst case -- the leak case, since
# normal draws ponynoblock ~50%): the Churner front-loads generations with no
# backpressure, so peak live actors ~= generations*group, at ~5.5 KB/worker, and
# group is SUPER-LINEAR (a group of g is g actors each holding a g-element tag
# array). The worst drawn case -- generations 8000 * group 16 = 128k workers, with
# the largest cyclic chains/ttl and a fresh 4096-byte string -- measures ~1.2 GB
# peak RSS and fits under the 4 GB DEFAULT_MEM_LIMIT_MB with ~3x margin, CD on and
# off. group 32 is deliberately excluded: same worker count costs ~3x the memory.
#
# Time bound: total messages = generations * chains * (ttl+1), so cyclic chains/ttl
# are kept SMALL (their own buckets below, NOT the mesh's magnitude buckets, which
# would explode the count) -- max ~1.1M messages, trivially within the soak budget.
# Small chains/ttl also let a generation drain and be collected fast, so collection
# races creation rather than piling up. COUPLING: these bounds are a property of the
# engine's per-worker footprint and the front-loading Churner -- re-measure if the
# cyclic actor's fields or the Churner's pacing change.
CYCLIC_GROUPS = [2, 4, 8, 16]
CYCLIC_GENERATION_BUCKETS = {
    "small": (10, 800),
    "medium": (801, 3000),
    "large": (3001, 8000),
}
CYCLIC_CHAINS_BUCKETS = {"small": (1, 2), "medium": (3, 5), "large": (6, 8)}
CYCLIC_TTL_BUCKETS = {"small": (1, 4), "medium": (5, 10), "large": (11, 16)}

# Normal-mode backpressure draws (the `backpressure` workload). A star of
# `producers` actors floods one consumer that explicitly applies/releases runtime
# backpressure every `apply_every` received messages, muting its producers via the
# UNDER_PRESSURE path the mesh's automatic OVERLOAD muting never reaches.
#
# Memory is NOT a governing bound here -- the opposite of cyclic. The consumer's
# mailbox stays bounded by the runtime's automatic OVERLOAD muting irrespective of
# whether explicit backpressure engages or how `apply_every` is set (measured ~11 MB
# peak RSS even in a pure-flood config where explicit BP never fires). The producer
# cap is for muteset-depth stress (one receiver's muteset up to `producers` deep)
# and pre-flood actor-creation cost, NOT mailbox memory; RLIMIT_AS stays the free
# backstop.
#
# Time governs the bounds: total messages = producers * messages, so `messages` is
# bucketed small->large for magnitude variety (the #5601 philosophy). The worst
# drawn case -- 256 producers * 400000 messages = 102.4M messages, fresh 4096-byte
# string, apply_every 50 -- measures ~6 min wall and ~404 MB peak RSS on the
# calibration host: under the 100-min per-run cap (~17x margin) and ~10x under the
# 4 GB RLIMIT_AS, and shorter than the mesh's ~15-20 min worst case. Most draws are
# far shorter (small/medium buckets dominate). `apply_every` is a set: smaller =
# more frequent apply/release toggling = more mute/unmute churn (the worst case
# above logged ~32M mutes / ~299k explicit applies). COUPLING: the time bound is a
# property of the engine's per-message cost -- re-measure if the Producer/Consumer
# per-message work changes. Calibrated, not provisional.
BACKPRESSURE_PRODUCERS = [16, 32, 64, 128, 256]
BACKPRESSURE_MESSAGE_BUCKETS = {
    "small": (1000, 20000),
    "medium": (20001, 100000),
    "large": (100001, 400000),
}
BACKPRESSURE_APPLY_EVERY = [50, 200, 1000]

# Normal-mode iso-handoff draws (the `iso` workload). The `mesh` topology, but the
# val payload is replaced by a freshly built nested `iso` graph and consumed
# hop-to-hop; the receiver acquires a foreign MUTABLE multi-object subgraph -- the
# new ORCA coverage. iso has its OWN cargo knobs (it does NOT draw the val payload):
# a `Parcel` tree of `depth` levels with `breadth` children per node, each node a
# separate allocation holding a `node_size`-byte array. iso reuses NORMAL_PINGERS
# for its actor count (pingers=1 is the self-handoff, bounded by PR #5594's
# self-send pin).
#
# Graph shape: ISO_DEPTHS x ISO_BREADTHS gives node counts in {1,2,3,4,7,15} (a flat
# node through a 15-node tree), so the swarm spans flat/chain/tree. depth is capped
# at 4 to keep the GC-trace recursion shallow.
#
# Memory bound (CALIBRATED on the engine, 4 GB RLIMIT_AS, real-parallel, ~16
# threads): each parcel is owned by the Dispatcher and acquired by every carrier it
# passes, so the owner's GC-acquire backlog scales with the ACQUIRE-FLOOD =
# chains * ttl * node_count. The OOM cliff is non-monotonic near the edge (7 nodes
# at 2285 chains crashed at the same ~256k acquire events where other shapes were
# safe), so the budget is set CONSERVATIVELY: at K=96000 the worst measured is
# ~160 MB (pingers=16, 15-node tree), every shape/pinger combo is safe, and 2x the
# budget still did not crash -- ~25x margin under 4 GB. The per-run `chains` draw is
# CLAMPED to K/(ISO_TTL_MAX*node_count) (iso_chains_cap), so a 1-node run goes high-
# volume (up to ~6000 chains) and a 15-node run stays low-volume (~400), both safe.
# COUPLING: the budget is a property of the Dispatcher's front-loaded injection and
# the per-node acquire footprint -- re-measure if either changes. Calibrated.
ISO_NODE_SIZES = [1, 8, 64, 256, 1024]
ISO_DEPTHS = [1, 2, 3, 4]
ISO_BREADTHS = [0, 1, 2]
ISO_TTL_BUCKETS = {"small": (1, 4), "medium": (5, 10), "large": (11, 16)}
ISO_TTL_MAX = 16
# Pre-clamp chains magnitude buckets (max = the 1-node cap K/ISO_TTL_MAX = 6000);
# iso_chains_cap then clamps per run to the drawn graph's safe limit.
ISO_CHAINS_BUCKETS = {"small": (50, 1000), "medium": (1001, 3000),
                      "large": (3001, 6000)}
ISO_ACQUIRE_BUDGET = 96000

# Normal-mode actorref draws (the `actorref` workload). The `mesh` topology, but the
# cargo is a FRESH `Referent` actor's tag per chain, forwarded hop-to-hop and never
# stored, so each forward drives ORCA's actor-reference ACQUIRE (and the dropped ref a
# RELEASE) -- the actor-reference machinery the other workloads leave cold (measured:
# they hit acquire_actor 0-1 times a run). actorref reuses NORMAL_PINGERS for its relay
# count; pingers=1 is the self-referral stress point (highest acquire rate,
# acquire == chains*ttl), measured memory-safe -- NO #5594-style flood, since fresh
# referents move hop-to-hop and are collected rather than one object re-borrowed across
# sweeps.
#
# Memory bound (CALIBRATED on the engine, normal parallel, 4 GB RLIMIT_AS): peak RSS
# tracks CHAINS -- the front-loaded in-flight referent + acquire/release backlog at the
# single owner (the Referrer) -- and is ~FLAT in ttl. Measured chains(ttl=16):
# 10k->25MB, 40k->100MB, 100k->156MB, 200k->528MB; ttl(chains=40k): 4->83MB, 16->104MB,
# 48->109MB. So the bound is a chains cap, NOT a chains*ttl clamp (unlike iso). The
# large bucket maxes at 34000 chains (~100MB peak, ~40x margin under 4 GB). ttl stays
# small (acquire volume = chains*ttl governs run TIME; 34000*16 = 578k messages), so it
# reuses iso's small ttl range. The worst normal run measured ~1.3s single-threaded
# (pingers=1, chains=34000, ttl=16, aggressive GC), far under the 300s no-progress
# window -- so actorref never needs a heartbeat (its 578k-message max stays under
# _Heartbeat._min, like small mesh/iso runs; the Relay's heartbeat plumbing is inert for
# the drawn range and only engages above the chains ceiling). COUPLING: this bound is a
# property of the Referrer's front-loaded injection and the per-referent footprint --
# re-measure if either changes. Calibrated.
ACTORREF_CHAINS_BUCKETS = {"small": (50, 2000), "medium": (2001, 15000),
                           "large": (15001, 34000)}
# actorref reuses iso's ttl envelope. This is an ALIAS, not a copy: editing
# ISO_TTL_BUCKETS moves actorref's ttl floor/range too. Each workload asserts its own
# `>= 1` ttl floor through its own name (the tests read ISO_TTL_BUCKETS and
# ACTORREF_TTL_BUCKETS separately), so splitting this alias into two distinct buckets
# later leaves each floor guard pinned to the right one.
ACTORREF_TTL_BUCKETS = ISO_TTL_BUCKETS
# Ceiling guard: peak RSS tracks max chains. Unlike the cyclic/backpressure/iso NORMAL
# ceilings -- which sit ~1x above their drawn max because their cost near that max is
# non-linear (cyclic's super-linear workers, iso's non-monotonic acquire-flood) or
# time-bound (backpressure), so any bump past the calibrated max warrants a fresh
# measurement -- actorref's drawn max (34000 chains, ~100MB) has ~40x headroom under
# the 4 GB cap and its cost is linear, so this is a looser 2x re-measure trigger
# (matching the 2x SYSTEMATIC ceilings). The tests compare with a strict `<`, so a bump
# that REACHES it -- an exact doubling of the chains bucket max -- trips the re-measure,
# not only a bump strictly past it.
ACTORREF_CHAINS_CEILING = 34000 * 2


# Systematic-mode workload-kind weights for the 5-way draw (draw_systematic_workload),
# mirroring normal mode's split. mesh is the baseline widest-surface workload and stays
# the larger run; cyclic, backpressure, iso, and actorref each target one
# otherwise-untouched subsystem and are kept SMALL (below) so the serialized double-run
# soak costs no more per seed than the mesh-only draw it replaces. A run rolls mesh
# below MESH_P, cyclic below MESH_P + CYCLIC_P, backpressure below
# MESH_P + CYCLIC_P + BP_P, iso below MESH_P + CYCLIC_P + BP_P + ISO_P, else actorref.
# Backpressure's systematic determinism is verified: its muting/unmuting arrival order
# reproduces under replay (see the per-work ORDER_SIG fold in main.pony and
# .known-couplings/backpressure-workload-muting.md). actorref's rides on the same
# routing interleaving as mesh/iso, and it is the heaviest exerciser of the id-sorted
# actor acquire/release drain (see .known-couplings/systematic-testing-send-ordering.md).
SYSTEMATIC_MESH_P = 0.4
SYSTEMATIC_CYCLIC_P = 0.15
SYSTEMATIC_BACKPRESSURE_P = 0.15
SYSTEMATIC_ISO_P = 0.15   # actorref gets the remaining 1 - the four above

# Systematic cyclic/iso are deliberately SMALL. The systematic determinism oracle's
# value is exercising each kind's message ORDERING under reproducible replay, which
# small runs do; normal mode carries the large-magnitude cyclic/iso runs. The caps
# are calibrated (serialized, on a systematic ponyc) so no drawn run runs long: the
# worst cyclic is ~0.6s and the worst iso ~0.4s single, comparable to the worst mesh
# (~0.4s), and a full mixed --count soak measures CHEAPER per seed than the old
# mesh-only draw (whose chains-1-400 forward runs dominate). That keeps the daily
# systematic soak within the macOS runner's job-time budget (that lane alerts on
# non-completion; see the stress workflows).
#
# cyclic cost scales with generations*group (actors created + collected). Normal's
# gen 8000 measures ~47-61s serialized -- run twice by the determinism oracle it
# blows the per-run watchdog -- so systematic caps generations far below it; gen 50
# at group 16 measures ~0.6s worst. cyclic chains/ttl reuse the normal cyclic
# envelope (1-8 / 1-16), already systematic-small.
SYSTEMATIC_CYCLIC_GENERATION_MAX = 50

# iso serialized cost is driven by the ACQUIRE-FLOOD (chains * ttl * node_count) --
# the quantity iso_chains_cap bounds for memory -- not by chains alone: at a fixed
# message count the 15-node graph runs ~5x the 1-node graph (more acquires per hop),
# so the worst iso is the 15-node/chains-400 corner (~0.4s, ~= mesh worst), NOT the
# 1-node corner. systematic caps iso chains at 400 (< normal's 6000); 400 equals the
# 15-node acquire cap (400*ISO_TTL_MAX*15 = ISO_ACQUIRE_BUDGET), so the iso_chains_cap
# clamp in draw_systematic_workload is non-binding at this max -- kept to ENFORCE the
# acquire/memory invariant if the cap is raised (re-measure the 15-node corner then).
# iso ttl reuses ISO_TTL_MAX (the acquire-cap math assumes ttl <= it).
SYSTEMATIC_ISO_CHAINS_MAX = 400

# Systematic backpressure is TIME-bound like cyclic/iso: kept small so the serialized
# double-run costs no more per seed than the other kinds. Cost is driven by the total
# work = producers * messages (~20 us/message serialized on a debug systematic ponyc).
# The drawn worst -- 64 producers * 300 messages = 19200, with apply_every=1 (maximal
# mute/unmute churn) -- measures ~0.45s single, in the band of the worst cyclic (~0.6s)
# and iso (~0.4s); the ceiling guard in stress_common_test.py pins 2x that product (the
# re-measure trigger, ~1.0s single).
#
# producers spans the single-producer baseline (a depth-1 muteset: conservation + the
# apply/release/single-mute path, untested by normal mode which starts at 16) through a
# 64-deep muteset. The arrival-race sensitivity the per-work ORDER_SIG fold captures
# comes from producers >= 4 -- with one producer the arrival order is trivial, so that
# draw only exercises the reproduce/conservation checks, not a muting race. messages is
# drawn for magnitude variety. apply_every is small so muting FIRES even on small runs:
# apply_every=1 applies/releases on every received message (maximal churn), while a
# small-total draw with the largest apply_every exercises the pure-flood path where
# explicit backpressure never engages. COUPLING: the time bound is a property of the
# engine's per-message cost -- re-measure if Producer/Consumer per-message work changes.
SYSTEMATIC_BACKPRESSURE_PRODUCERS = [1, 4, 16, 64]
SYSTEMATIC_BACKPRESSURE_MESSAGES_MAX = 300
SYSTEMATIC_BACKPRESSURE_APPLY_EVERY = [1, 10, 50, 200]

# Systematic actorref is TIME-bound like cyclic/iso/backpressure. Serialized cost is
# driven by total hops = chains * (ttl + 1) (~20 us/message on a debug systematic
# ponyc). The drawn worst -- 400 chains * 16 ttl = 6400 hops -- measures ~0.5s single,
# in the band of the worst cyclic (~0.6s) and iso (~0.4s). Chains are capped far below
# normal's 34000 (400 chains serialized ~0.5s; 2000 ~4s would blow the per-run budget).
# actorref's coverage value -- driving the actor-ref ACQUIRE/RELEASE proportional to
# chains*ttl -- is exercised fully at this small scale under the reproducible oracle;
# normal mode carries the large-magnitude runs. ttl reuses ISO_TTL_MAX; ttl >= 1 is
# required (a zero-hop (ttl=0) chain injects but never forwards, so it drives no
# acquire). No memory clamp: at 400 chains peak RSS is a few MB (memory tracks chains,
# calibrated in the normal notes above). Ceiling guard in stress_common_test.py pins 2x
# the worst hop
# product (the re-measure trigger).
SYSTEMATIC_ACTORREF_CHAINS_MAX = 400

# Per-kind (chains, ttl) inclusive ranges for the systematic draw. One place to read
# every systematic range. mesh keeps its historical uniform ranges; cyclic reuses the
# normal cyclic envelope; iso is capped as above; backpressure has NO chains/ttl but
# still draws them (drawn-then-ignored, as normal mode does) so the two randint calls
# stay uniform across kinds. draw_systematic_workload makes the SAME two randint calls
# for every kind (the range differs, the call count does not -- fixed consumption).
SYSTEMATIC_WORKLOAD_RANGES = {
    "mesh":         ((1, 400), (0, 48)),
    "cyclic":       ((1, 8), (1, 16)),
    "backpressure": ((1, 400), (0, 48)),  # drawn-then-ignored (no chains/ttl)
    "iso":          ((1, SYSTEMATIC_ISO_CHAINS_MAX), (1, ISO_TTL_MAX)),
    "actorref":     ((1, SYSTEMATIC_ACTORREF_CHAINS_MAX), (1, ISO_TTL_MAX)),
}


def iso_node_count(depth, breadth):
    """Total nodes in a `Parcel` tree of `depth` levels and `breadth` children per
    node: 1 + breadth + breadth^2 + ... + breadth^(depth-1). depth 1 or breadth 0 is
    a single node. Matches the engine's recursive `Parcel` constructor."""
    if depth <= 1 or breadth == 0:
        return 1
    return sum(breadth ** level for level in range(depth))


def iso_chains_cap(depth, breadth):
    """The safe `chains` ceiling for a drawn graph shape: the acquire-flood budget
    divided by (ISO_TTL_MAX * node_count). Used to CLAMP the drawn chains so no run
    exceeds chains * ttl * node_count <= ISO_ACQUIRE_BUDGET (ttl <= ISO_TTL_MAX), the
    calibrated memory-safe envelope. node_count==1 gives the max, 6000."""
    return ISO_ACQUIRE_BUDGET // (ISO_TTL_MAX * iso_node_count(depth, breadth))


def draw_bucketed(rng, buckets):
    """Draw a small/medium/large bucket at 25/50/25, then a uniform value within it.
    Exactly two rng draws (the bucket roll, then the value), and that order is part
    of the seed-stability contract. `buckets` is a name->(lo, hi) inclusive dict with
    small/medium/large keys."""
    roll = rng.random()
    if roll < 0.25:
        lo, hi = buckets["small"]
    elif roll < 0.75:
        lo, hi = buckets["medium"]
    else:
        lo, hi = buckets["large"]
    return rng.randint(lo, hi)


def draw_payload(rng):
    """Draw payload kind, size, and mode. Returns (payload, size, mode).

    Size is drawn freely from the full set -- a big run is no longer denied a large
    string size, because run time is bounded by clamp_ttl (which trims ttl) rather
    than by restricting size. Exactly three rng draws -- kind, mode, size -- in that
    fixed order, so the draw stream is stable."""
    kind = rng.choice(["string", "u64"])
    mode = rng.choice(["forward", "fresh"])
    size = STRING_SIZES[int(rng.random() * len(STRING_SIZES))]
    return kind, size, mode


def draw_workload(rng):
    """Draw the workload kind plus EVERY kind's specific shape, returning
    (workload, generations, group, producers, messages, apply_every, node_size,
    depth, breadth). ALWAYS the same fixed sequence of logical draws regardless of
    the rolled kind -- the kind roll, then the cyclic shape (bucketed generations +
    group choice), then the backpressure shape (producers choice + bucketed messages
    + apply_every choice), then the iso cargo shape (node_size + depth + breadth
    choices). No branch on the rolled kind, so the kind changes which keys the config
    later emits, never how many draws this makes (the fixed-consumption discipline;
    "logical draws" because a randint's underlying bit consumption varies with its
    value, but the call sequence does not). The mesh/iso/actorref `pingers` field and
    the per-kind bucketed chains/ttl are drawn separately in resolve_config, as
    before. actorref adds NO cargo draw of its own -- it reuses pingers/chains/ttl and
    draws-then-ignores the payload like iso, so the call count is unchanged and only
    the kind-threshold reweight remaps historical seeds. Every kind's params are drawn
    even when another kind is rolled (and then ignored) to keep that sequence fixed."""
    roll = rng.random()
    if roll < WORKLOAD_MESH_P:
        workload = "mesh"
    elif roll < (WORKLOAD_MESH_P + WORKLOAD_CYCLIC_P):
        workload = "cyclic"
    elif roll < (WORKLOAD_MESH_P + WORKLOAD_CYCLIC_P + WORKLOAD_BP_P):
        workload = "backpressure"
    elif roll < (WORKLOAD_MESH_P + WORKLOAD_CYCLIC_P + WORKLOAD_BP_P
                 + WORKLOAD_ISO_P):
        workload = "iso"
    else:
        workload = "actorref"
    generations = draw_bucketed(rng, CYCLIC_GENERATION_BUCKETS)
    group = rng.choice(CYCLIC_GROUPS)
    producers = rng.choice(BACKPRESSURE_PRODUCERS)
    messages = draw_bucketed(rng, BACKPRESSURE_MESSAGE_BUCKETS)
    apply_every = rng.choice(BACKPRESSURE_APPLY_EVERY)
    node_size = rng.choice(ISO_NODE_SIZES)
    depth = rng.choice(ISO_DEPTHS)
    breadth = rng.choice(ISO_BREADTHS)
    return (workload, generations, group, producers, messages, apply_every,
            node_size, depth, breadth)


def draw_systematic_workload(rng, program_seed):
    """The systematic-mode workload shape: the program seed (passed, not drawn) plus
    a drawn kind -- `mesh | cyclic | backpressure | iso | actorref` -- and, drawn
    UNCONDITIONALLY for every kind, all of every kind's shape
    fields. Only the rolled kind's fields are emitted, so the engine never receives a
    contradictory flag. Like draw_workload, it makes the SAME fixed sequence of draws
    regardless of the rolled kind (the iso `min()` clamp consumes no rng), so a future
    kind-dependent draw -- which would remap seeds -- is caught by the call-count test.

    `payload-mode` is deliberately NOT drawn here -- the systematic driver draws it
    AFTER its runtime-knob draws and BEFORE draw_max_threads() (see
    orchestrate_systematic.resolve_config); that split is part of the seed-stability
    contract. Move any draw and every seed remaps (breaking historical --replay):
    resolve_config(0, 8) is pinned in orchestrate_systematic_test.py and this
    function's Random(0) draw is pinned in stress_common_test.py.

    Systematic keeps cyclic/iso small (SYSTEMATIC_CYCLIC_GENERATION_MAX,
    SYSTEMATIC_ISO_CHAINS_MAX) so the serialized double-run soak costs no more per
    seed than the mesh-only draw it replaces; normal mode carries the large-magnitude
    cyclic/iso runs. mesh keeps its historical uniform ranges.
    """
    roll = rng.random()
    if roll < SYSTEMATIC_MESH_P:
        kind = "mesh"
    elif roll < SYSTEMATIC_MESH_P + SYSTEMATIC_CYCLIC_P:
        kind = "cyclic"
    elif roll < SYSTEMATIC_MESH_P + SYSTEMATIC_CYCLIC_P + SYSTEMATIC_BACKPRESSURE_P:
        kind = "backpressure"
    elif roll < (SYSTEMATIC_MESH_P + SYSTEMATIC_CYCLIC_P + SYSTEMATIC_BACKPRESSURE_P
                 + SYSTEMATIC_ISO_P):
        kind = "iso"
    else:
        kind = "actorref"
    # Every kind's cargo is drawn on every roll (fixed consumption); only the rolled
    # kind's fields are emitted below. pingers -> cyclic shape -> backpressure shape ->
    # iso cargo, then the per-kind chains/ttl (SAME two randint calls for every kind;
    # the range differs, the call count does not) -> payload kind/size. The cargo draw
    # order mirrors draw_workload's.
    pingers = rng.choice(NORMAL_PINGERS)
    generations = rng.randint(1, SYSTEMATIC_CYCLIC_GENERATION_MAX)
    group = rng.choice(CYCLIC_GROUPS)
    producers = rng.choice(SYSTEMATIC_BACKPRESSURE_PRODUCERS)
    messages = rng.randint(1, SYSTEMATIC_BACKPRESSURE_MESSAGES_MAX)
    apply_every = rng.choice(SYSTEMATIC_BACKPRESSURE_APPLY_EVERY)
    node_size = rng.choice(ISO_NODE_SIZES)
    node_depth = rng.choice(ISO_DEPTHS)
    node_breadth = rng.choice(ISO_BREADTHS)
    (chains_lo, chains_hi), (ttl_lo, ttl_hi) = SYSTEMATIC_WORKLOAD_RANGES[kind]
    chains = rng.randint(chains_lo, chains_hi)
    ttl = rng.randint(ttl_lo, ttl_hi)
    payload = rng.choice(["string", "u64"])
    payload_size = rng.choice(STRING_SIZES)

    # Emit only the rolled kind's fields (plus the always-present seed + workload).
    # mesh/cyclic/backpressure carry the val payload; iso and actorref carry NO payload
    # (drawn-then-ignored, like normal mode) -- iso has its own cargo knobs, actorref's
    # cargo is a fresh actor built in the engine. pingers is a mesh/iso/actorref field;
    # backpressure has producers/messages/apply-every and NO chains/ttl/pingers.
    workload = {"seed": program_seed, "workload": kind}
    if kind == "cyclic":
        workload["generations"] = generations
        workload["group"] = group
        workload["chains"] = chains
        workload["ttl"] = ttl
        workload["payload"] = payload
        workload["payload-size"] = payload_size
    elif kind == "backpressure":
        # chains/ttl were drawn above (fixed consumption) and are ignored here.
        workload["producers"] = producers
        workload["messages"] = messages
        workload["apply-every"] = apply_every
        workload["payload"] = payload
        workload["payload-size"] = payload_size
    elif kind == "iso":
        # Clamp chains to the memory-safe acquire budget for the drawn graph shape
        # (chains * ttl * node_count <= ISO_ACQUIRE_BUDGET, ttl <= ISO_TTL_MAX). At
        # SYSTEMATIC_ISO_CHAINS_MAX = 400 this is non-binding, but it enforces the
        # invariant if that cap is raised. No rng -- consumption stays fixed.
        chains = min(chains, iso_chains_cap(node_depth, node_breadth))
        workload["pingers"] = pingers
        workload["chains"] = chains
        workload["ttl"] = ttl
        workload["node-size"] = node_size
        workload["node-depth"] = node_depth
        workload["node-breadth"] = node_breadth
    elif kind == "actorref":
        # actorref reuses the mesh shape (pingers/chains/ttl) but carries NO payload
        # (its cargo is a fresh actor tag per chain, drawn nowhere -- built in the
        # engine). Its systematic chains/ttl range ((1, SYSTEMATIC_ACTORREF_CHAINS_MAX),
        # (1, ISO_TTL_MAX)) was already applied by the randint calls above. No memory
        # clamp: peak RSS tracks chains, tiny at the 400 cap (see the normal notes).
        workload["pingers"] = pingers
        workload["chains"] = chains
        workload["ttl"] = ttl
    else:                              # mesh
        workload["pingers"] = pingers
        workload["chains"] = chains
        workload["ttl"] = ttl
        workload["payload"] = payload
        workload["payload-size"] = payload_size
    return workload


def build_argv(binary, config):
    """Full command line for a resolved config: workload args first, then runtime
    flags. A flag whose value is True is bare (`--ponynoscale`); anything else is
    `--key value`."""
    argv = [binary]
    for key, value in config["workload"].items():
        argv += ["--" + key, str(value)]
    for key, value in config["runtime"].items():
        if value is True:
            argv.append("--" + key)
        else:
            argv += ["--" + key, str(value)]
    return argv


def run_command(binary, config):
    """The full run command for a resolved config. There is deliberately no
    ASLR/setarch wrapper: systematic replay is layout-independent since
    #5566/#5570, and the normal mode is non-reproducible by design, so this is
    exactly the engine argv on every platform."""
    return build_argv(binary, config)


def parse_result(stdout):
    """Extract the engine's RECEIVED/SENT/EXPECTED/ORDER_SIG line, or {}."""
    result = {}
    for key in ("RECEIVED", "SENT", "EXPECTED", "ORDER_SIG"):
        match = re.search(key + r"=(\d+)", stdout)
        if match is not None:
            result[key.lower()] = match.group(1)
    return result


def run(cmd, **kwargs):
    info("+ " + " ".join(shlex.quote(c) for c in cmd))
    return subprocess.run(cmd, **kwargs)


def probe_max_threads(binary, seed_args):
    """Probe the runtime's physical core count -- the ceiling `--ponymaxthreads`
    is checked against. Ask the engine for an impossible thread count and parse
    the count the runtime rejects with, so we match the runtime's own
    (SMT-collapsing) definition exactly rather than guessing from os.cpu_count(),
    which is logical and would over-count on an SMT host.

    `seed_args` carries the mode-specific flags the runtime needs to start: the
    systematic driver passes `--ponysystematictestingseed 1` (its runtime requires
    a seed); the normal driver passes none (a normal runtime rejects that flag).
    Falls back to os.cpu_count() if the rejection can't be parsed -- e.g. on a
    platform whose reject message differs (Windows is unverified; the fallback
    covers it)."""
    cmd = [binary, "--ponymaxthreads", "1000000", "--ponynoscale"] + list(seed_args)
    try:
        result = subprocess.run(cmd, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=30)
        text = (result.stdout.decode(errors="replace")
                + result.stderr.decode(errors="replace"))
        match = re.search(r"> (\d+)\)", text)
        if match is not None:
            return int(match.group(1))
    except subprocess.TimeoutExpired:
        pass
    fallback = os.cpu_count() or 1
    info("note: could not probe physical cores from the runtime; using "
         "os.cpu_count()=%d" % fallback)
    return fallback


def ponyc_version(ponyc):
    """Capture `ponyc --version` verbatim and in full (it is the primary
    provenance; the embedded git SHA is authoritative even past a tree move)."""
    result = subprocess.run([ponyc, "--version"], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    return result.stdout.decode(errors="replace").strip()


def is_systematic_build(binary):
    """True if the compiled engine is a `use=systematic_testing` build.

    Guards a real footgun for the normal driver: point it at a systematic ponyc
    and -- with no `--ponysystematictestingseed` -- the systematic runtime runs
    SERIALIZED off a wall-clock seed, silently defeating real parallelism while
    conservation still passes and CI stays green. A systematic runtime prints a
    "Systematic testing using seed" banner at startup, so run a trivial workload
    and look for it. A normal runtime just runs the workload and exits."""
    try:
        result = subprocess.run(
            [binary, "--pingers", "1", "--chains", "1", "--ttl", "0",
             "--ponynoscale"], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            timeout=30)
    except subprocess.TimeoutExpired:
        # A trivial ttl=0 run should finish instantly; if it somehow hangs we
        # can't classify, so don't block startup -- the per-seed watchdog catches
        # a genuinely hung engine. (Mirrors probe_max_threads' graceful fallback.)
        info("note: could not probe build type (engine did not finish); "
             "assuming normal")
        return False
    text = (result.stdout.decode(errors="replace")
            + result.stderr.decode(errors="replace"))
    return "Systematic testing using seed" in text


def compile_engine(ponyc, out_dir):
    """Compile the engine once with the provided ponyc (always `-d`). stdlib
    resolves from REPO_ROOT/packages. The produced binary is `generative` on POSIX
    and `generative.exe` on Windows; `os.X_OK` is meaningless on Windows, so
    existence (os.path.isfile) is the cross-platform check."""
    env = dict(os.environ, PONYPATH=os.path.join(REPO_ROOT, "packages"))
    result = run([ponyc, "-d", "-b", BINARY_NAME, "--pic", "-o", out_dir,
                  SOURCE_DIR], env=env)
    if result.returncode != 0:
        die("compiling the engine failed")
    binary = os.path.join(out_dir, BINARY_NAME)
    if os.name == "nt":
        binary += ".exe"
    if not os.path.isfile(binary):
        die("engine binary not produced at " + binary)
    return binary


class RunResult:
    """How a single run ended: outcome, exit status, and captured streams.

    `signal` is a diagnostic, recorded for the bundle and never parsed back, so
    its type intentionally differs by runner: the direct runner (run_once) sets a
    terminating signal NUMBER (int) or None; the lldb runner (run_under_lldb) sets
    the signal NAME (str, e.g. "SIGSEGV") or "crash" when lldb reports no number.
    """

    def __init__(self, outcome, returncode, signal, stdout, stderr):
        self.outcome = outcome  # "pass" | "fail" | "timeout"
        self.returncode = returncode
        self.signal = signal
        self.stdout = stdout
        self.stderr = stderr


def _rlimit_as_supported(platform, resource_available):
    """Whether to install the RLIMIT_AS address-space cap on this platform.

    An allowlist, not a denylist: the cap is applied only on Linux, the one
    TIER1 platform validated to honor RLIMIT_AS. Windows lacks the POSIX
    `resource` module entirely; macOS HAS it but Darwin does not honor
    RLIMIT_AS, and `setrlimit` raises there -- inside `preexec_fn` (post-fork,
    pre-exec) that aborts the spawn with `SubprocessError`, crashing every run
    before the engine starts rather than capping anything.

    Allowlisting is deliberate: that macOS crash is the failure mode of applying
    the cap on an unvalidated platform, so any platform we have not confirmed
    falls back to the host's OOM handling (as Windows already did) instead of
    risking the same crash. A new platform that honors RLIMIT_AS is added here
    explicitly once validated.

    Pure (platform + resource-availability in, bool out) so it is testable from
    any host regardless of where the suite runs."""
    return resource_available and (platform == "linux")


def _capture(argv, timeout, mem_limit_bytes, no_progress_seconds=None):
    """Run argv under a watchdog and (on Linux) an RLIMIT_AS cap; return
    (timed_out, returncode, stdout, stderr). Mechanism only -- the caller classifies.

    Two watchdog modes:
      * no_progress_seconds is None (SYSTEMATIC): `timeout` is a single total
        wall-clock limit -- the original behavior. Systematic runs are short and
        reproducible, and a systematic hang must still surface as a timeout (see
        .known-couplings/systematic-testing-park-sites.md), so this stays unchanged.
      * no_progress_seconds is set (NORMAL): the run is killed when it produces NO
        output for that long (a hang); a slow-but-advancing run -- which the engine's
        heartbeats keep printing through -- is NOT killed, so a legitimately long run
        finishes instead of false-failing. `timeout` is then an absolute backstop.
        Both kills return timed_out=True -- a no-progress kill IS a hang/timeout.

    On any kill we reap the whole process GROUP, not just the direct child: normal
    mode's direct child is lldb and the engine is its grandchild, so SIGKILLing only
    lldb would orphan a hung engine. `start_new_session` puts the child in its own
    group so killpg reaps the tree. The RLIMIT_AS cap is applied only where the OS
    honors it -- see _rlimit_as_supported (Windows/macOS fall back to host OOM)."""
    preexec = None
    if (mem_limit_bytes is not None) and _rlimit_as_supported(
            sys.platform, resource is not None):
        def set_limits():
            resource.setrlimit(resource.RLIMIT_AS,
                               (mem_limit_bytes, mem_limit_bytes))
        preexec = set_limits

    info("+ " + " ".join(shlex.quote(a) for a in argv))
    proc = subprocess.Popen(argv, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, preexec_fn=preexec,
                            start_new_session=(os.name == "posix"))
    if no_progress_seconds is None:
        try:
            stdout, stderr = proc.communicate(timeout=timeout)
            return (False, proc.returncode, _decode(stdout), _decode(stderr))
        except subprocess.TimeoutExpired:
            _kill_process_tree(proc)
            stdout, stderr = proc.communicate()
            return (True, None, _decode(stdout), _decode(stderr))
    return _watch_for_progress(proc, timeout, no_progress_seconds)


def _watchdog_should_kill(now, start, last_output, timeout, no_progress_seconds):
    """The normal-mode watchdog decision: kill if the run has been silent past the
    no-progress window (a hang) OR has run past the absolute backstop. A run that
    keeps producing output (last_output recent) survives indefinitely up to the
    backstop -- that is the "slow but progressing is not a hang" property."""
    return ((now - last_output) > no_progress_seconds) or ((now - start) > timeout)


def _watch_for_progress(proc, timeout, no_progress_seconds,
                        poll=time.monotonic, sleep=time.sleep):
    """Drain BOTH of proc's streams in reader threads -- concurrently, so a full
    stderr pipe (lldb is chatty) can't deadlock the child the way a single deferred
    reader would -- updating a last-output time on every line. Kill the process
    group on a no-progress gap (a hang) or the absolute `timeout` backstop;
    otherwise let the run finish. Returns (timed_out, returncode, stdout, stderr).
    `poll`/`sleep` are injectable so the watchdog logic is testable without real
    time."""
    last_output = [poll()]
    lock = threading.Lock()
    chunks = {"out": [], "err": []}

    def drain(stream, key):
        try:
            for line in iter(stream.readline, b""):
                chunks[key].append(line)
                with lock:
                    last_output[0] = poll()
        finally:
            stream.close()

    readers = [
        threading.Thread(target=drain, args=(proc.stdout, "out"), daemon=True),
        threading.Thread(target=drain, args=(proc.stderr, "err"), daemon=True),
    ]
    for t in readers:
        t.start()

    start = poll()
    timed_out = False
    while proc.poll() is None:
        now = poll()
        with lock:
            last = last_output[0]
        if _watchdog_should_kill(now, start, last, timeout, no_progress_seconds):
            _kill_process_tree(proc)
            timed_out = True
            break
        sleep(0.5)
    proc.wait()
    for t in readers:
        t.join(timeout=5)
    stdout = _decode(b"".join(chunks["out"]))
    stderr = _decode(b"".join(chunks["err"]))
    returncode = None if timed_out else proc.returncode
    return (timed_out, returncode, stdout, stderr)


def _kill_process_tree(proc):
    """Kill proc and, on POSIX, its whole session group -- so a hung lldb does not
    orphan the engine grandchild. Windows has no killpg, so it kills the direct
    child and leans on the job's container teardown for stragglers."""
    if os.name == "posix":
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            return
        except (ProcessLookupError, PermissionError):
            pass
    proc.kill()


def run_once(binary, config, timeout, mem_limit_bytes):
    """Direct run (no debugger): classify the outcome straight from the process
    exit code. A negative code is a terminating signal. Used by the systematic
    driver, whose runs reproduce -- a crash is re-run under a debugger locally
    from its seed. (The normal driver uses run_under_lldb instead; see there.)"""
    timed_out, returncode, stdout, stderr = _capture(
        run_command(binary, config), timeout, mem_limit_bytes)
    if timed_out:
        return RunResult("timeout", None, None, stdout, stderr)
    signal = -returncode if returncode < 0 else None
    outcome = "pass" if returncode == 0 else "fail"
    return RunResult(outcome, returncode, signal, stdout, stderr)


def lldb_argv(lldb, engine_argv):
    """Wrap the engine argv to run under `lldb --batch`. On a crash the on-crash
    commands (`frame variable`, `bt all`) print the crash site to the captured
    output and `quit 1` makes lldb exit non-zero, so the backtrace survives even
    when the run doesn't reproduce. POSIX is the careful path: the Pony runtime
    uses SIGUSR2 for scheduler wakeups (and SIGINT for shutdown), so lldb must pass
    those through WITHOUT stopping -- otherwise it halts on every scheduler signal
    and the run hangs. We stop at `main`, configure that pass-through, then
    continue. Windows has no such signals, so it just runs. Mirrors the
    `usedebugger=lldb` invocation in the Makefile / make.ps1.

    `register read` is deliberately omitted: we don't upload the failing binary,
    so raw register addresses can't be re-symbolized -- `frame variable` + `bt all`
    (symbolized at capture in a debug build) are the useful artifacts.

    COUPLING: the pass-through list (SIGINT, SIGUSR2) is complete only for today's
    socketless engine. If the engine ever opens a socket the runtime arms SIGPIPE,
    and lldb would stop on it -- pass it through here too, or the run hangs."""
    on_crash = ["--one-line-on-crash", "frame variable",
                "--one-line-on-crash", "bt all",
                "--one-line-on-crash", "quit 1"]
    if os.name == "posix":
        setup = ["--one-line", "breakpoint set --name main",
                 "--one-line", "run",
                 "--one-line", "process handle SIGINT --pass true --stop false",
                 "--one-line", "process handle SIGUSR2 --pass true --stop false",
                 "--one-line", "thread continue"]
    else:
        setup = ["--one-line", "run"]
    return [lldb, "--batch"] + setup + on_crash + ["--"] + engine_argv


def lldb_exit_code(output):
    """The engine's real exit code parsed from lldb --batch output ("Process N
    exited with status = M"), or None if it did not exit cleanly -- i.e. it
    crashed, lldb ran the on-crash backtrace and quit 1, and there is no
    exit-status line (the backtrace is in `output`). Matches the Makefile/make.ps1
    lldb exit-code extraction."""
    match = re.search(r"Process \d+ exited with status = (\d+)", output)
    return int(match.group(1)) if match is not None else None


def run_under_lldb(binary, config, lldb, timeout, mem_limit_bytes,
                   no_progress_seconds=None):
    """Run the engine under lldb so a crash leaves a backtrace in the captured
    output. The normal mode is non-reproducible (real parallelism), so an
    in-the-moment stack is the only crash artifact -- re-running the seed need not
    crash again. The engine's real exit code comes from lldb's exit-status line
    (conservation failures exit 1, a clean run exits 0); no such line means the
    program crashed, which is a failure with the backtrace already captured.

    `no_progress_seconds` (the normal driver sets it) makes the watchdog fire on a
    hang -- silence -- rather than on total length, so a slow run finishes; see
    _capture."""
    timed_out, _lldb_rc, stdout, stderr = _capture(
        lldb_argv(lldb, run_command(binary, config)), timeout, mem_limit_bytes,
        no_progress_seconds=no_progress_seconds)
    if timed_out:
        return RunResult("timeout", None, None, stdout, stderr)
    combined = stdout + "\n" + stderr
    # Check for a crash FIRST: lldb's on-crash dump (frame variable / bt all) can
    # itself contain an "exited with status" string, so parsing the exit code
    # first could misread a crash as a clean exit. A "stop reason = signal" line
    # appears only on an actual stop -- the SIGUSR2/SIGINT pass-through never stops
    # -- so its presence is the reliable crash signal. The backtrace is already in
    # stdout/stderr -> the bundle.
    crash = re.search(r"stop reason = signal (\w+)", combined)
    if crash is not None:
        return RunResult("fail", None, crash.group(1), stdout, stderr)
    code = lldb_exit_code(combined)
    if code is None:
        # No crash signal and no clean-exit line: an aborted/failed run with
        # nothing to parse. Treat as a failure.
        return RunResult("fail", None, "crash", stdout, stderr)
    return RunResult("pass" if code == 0 else "fail", code, None, stdout, stderr)


def _decode(raw):
    return raw.decode(errors="replace") if raw else ""


def bundle_for(config, version, use_flags, argv, limits, result):
    """Assemble the failure-bundle dict: everything needed to reproduce and
    attribute a run from the record alone. `systematic_seed` is included only when
    the config carries one (systematic mode); the normal mode has none."""
    bundle = {
        "master_seed": config["master_seed"],
        "program_seed": config["workload"]["seed"],
        "workload": config["workload"],
        "runtime_flags": config["runtime"],
        "cli": " ".join(shlex.quote(a) for a in argv),
        "ponyc_version": version,
        "use_flags": use_flags,
        "limits": limits,
        "outcome": result.outcome,
        "returncode": result.returncode,
        "signal": result.signal,
        "stdout": result.stdout,
        "stderr": result.stderr,
    }
    systematic_seed = config["runtime"].get("ponysystematictestingseed")
    if systematic_seed is not None:
        bundle["systematic_seed"] = systematic_seed
    return bundle


def write_bundle(out_dir, bundle):
    path = os.path.join(out_dir, "bundle-%d.json" % bundle["master_seed"])
    with open(path, "w") as handle:
        json.dump(bundle, handle, indent=2, sort_keys=True)
    return path


def summary_line(config, result):
    parsed = parse_result(result.stdout)
    shape = config["workload"]
    detail = "received=%s expected=%s sig=%s" % (
        parsed.get("received", "?"), parsed.get("expected", "?"),
        parsed.get("order_sig", "?"))
    # Each workload carries different shape fields: mesh/cyclic have chains/ttl,
    # backpressure has producers/messages/apply-every instead (no chains/ttl), iso
    # has pingers/chains/ttl + its own cargo knobs and NO payload, actorref has
    # pingers/chains/ttl and NO payload. Both modes emit a `workload` key now;
    # `get(..., "mesh")` still covers any older/hand-built config that omits it. Each
    # branch reads only the fields its kind carries -- iso and actorref need their own
    # arms so a run is not mislabeled `mesh` by the fall-through, and the shared
    # `payload` read is guarded (iso/actorref have none).
    wl = shape.get("workload", "mesh")
    if wl == "cyclic":
        kind = "cyclic generations=%d group=%d chains=%d ttl=%d" % (
            shape["generations"], shape["group"], shape["chains"], shape["ttl"])
    elif wl == "backpressure":
        kind = "backpressure producers=%d messages=%d apply_every=%d" % (
            shape["producers"], shape["messages"], shape["apply-every"])
    elif wl == "iso":
        kind = ("iso pingers=%d chains=%d ttl=%d node_size=%d depth=%d breadth=%d"
                % (shape["pingers"], shape["chains"], shape["ttl"],
                   shape["node-size"], shape["node-depth"], shape["node-breadth"]))
    elif wl == "actorref":
        kind = "actorref relays=%d chains=%d ttl=%d" % (
            shape["pingers"], shape["chains"], shape["ttl"])
    else:
        kind = "mesh pingers=%d chains=%d ttl=%d" % (
            shape["pingers"], shape["chains"], shape["ttl"])
    return "[seed %d] %s (%s payload=%s) %s" % (
        config["master_seed"], result.outcome.upper(), kind,
        shape.get("payload", "none"), detail)


def execute(binary, config, version, use_flags, out_dir, timeout,
            mem_limit_bytes, runner=run_once):
    """Run one seed once; tee output, write a bundle on non-pass, return the
    RunResult. `runner` is the run strategy the driver injects, with signature
    `(binary, config, timeout, mem_limit_bytes) -> RunResult`: the systematic
    driver takes the default direct `run_once`; the normal driver passes an
    lldb-wrapped runner (see run_under_lldb). The captured stdout/stderr (which
    includes any crash backtrace) go to both the log and the bundle."""
    result = runner(binary, config, timeout, mem_limit_bytes)
    info(summary_line(config, result))
    if result.stdout:
        info(result.stdout.rstrip("\n"))
    if result.stderr:
        info(result.stderr.rstrip("\n"))
    if result.outcome != "pass":
        argv = run_command(binary, config)
        limits = {"timeout_seconds": timeout,
                  "mem_limit_bytes": mem_limit_bytes}
        path = write_bundle(out_dir,
                            bundle_for(config, version, use_flags, argv, limits,
                                       result))
        info("wrote failure bundle: " + path)
    return result


def resolve_seeds(args):
    """The explicit seed list from --master-seed / --replay / --count / --seeds.
    (The normal driver's wall-clock --budget-seconds loop is separate -- it
    generates seeds incrementally rather than from a fixed list.)"""
    if args.master_seed is not None:
        return [args.master_seed]
    if args.replay is not None:
        return [args.replay]
    if args.count is not None:
        return list(range(args.start, args.start + args.count))
    try:
        return [int(token) for token in args.seeds.split(",")]
    except ValueError:
        die("bad --seeds (expected comma-separated integers): " + args.seeds)
