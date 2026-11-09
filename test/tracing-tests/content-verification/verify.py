#!/usr/bin/env python3
"""Verify trace output content across runtime category toggle phases.

Reads a Chrome Trace JSON file produced by the content-verification test
program and checks the per-thread monotonic-transition invariant: once a
scheduler thread applies a category-change broadcast, it never emits
old-category events again.

The test program runs four phases:

  1. ActorBehaviorTracing on
  2. switch to SchedulerStealingTracing
  3. all categories off
  4. ActorBehaviorTracing on again

So each scheduler thread's event sequence across the whole trace must
match: actor_behavior* steal* actor_behavior* — no interleaving, no
regression.  This is always true regardless of how long the broadcast
takes to propagate, so there is no settle margin or timing sensitivity.
"""

import json
import sys
from collections import defaultdict


def load_events(path):
    with open(path) as f:
        text = f.read().rstrip().rstrip(",").rstrip()
        if not text.endswith("]"):
            text += "\n]"
        return json.loads(text)


def classify(name):
    if name in ("actor_behavior_run", "actor_behavior_schedule"):
        return "ab"
    if name in ("steal_successful", "steal_failure_round"):
        return "steal"
    return None


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <ponytrace.json>", file=sys.stderr)
        sys.exit(1)

    events = load_events(sys.argv[1])

    pass_count = 0
    fail_count = 0

    def check(label, condition):
        nonlocal pass_count, fail_count
        if condition:
            print(f"  PASS {label}")
            pass_count += 1
        else:
            print(f"  FAIL {label}")
            fail_count += 1

    # Group classified events by thread, sorted by timestamp.
    by_tid = defaultdict(list)
    has_ab = False
    has_steal = False
    for ev in events:
        kind = classify(ev.get("name", ""))
        if kind is not None:
            by_tid[ev["tid"]].append((ev["ts"], kind))
            if kind == "ab":
                has_ab = True
            else:
                has_steal = True

    check("actor_behavior events present", has_ab)
    check("steal events present", has_steal)

    # Per-thread monotonicity: ab* steal* ab*
    # State machine: AB1 → STEAL → AB2.  Any steal after AB2 is a violation.
    for tid in sorted(by_tid):
        evs = sorted(by_tid[tid])
        state = "AB1"
        for ts, kind in evs:
            if state == "AB1":
                if kind == "steal":
                    state = "STEAL"
            elif state == "STEAL":
                if kind == "ab":
                    state = "AB2"
            elif state == "AB2":
                if kind == "steal":
                    print(f"  FAIL tid {tid}: steal event at {ts} us "
                          f"after actor_behavior resumed (state AB2)")
                    fail_count += 1
                    break
        else:
            ab_count = sum(1 for _, k in evs if k == "ab")
            steal_count = sum(1 for _, k in evs if k == "steal")
            print(f"  PASS tid {tid}: {ab_count} actor_behavior, "
                  f"{steal_count} steal, pattern ab*·steal*·ab*")
            pass_count += 1

    print(f"\n{pass_count} passed, {fail_count} failed")
    sys.exit(1 if fail_count > 0 else 0)


if __name__ == "__main__":
    main()
