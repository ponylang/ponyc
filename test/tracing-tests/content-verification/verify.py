#!/usr/bin/env python3
"""Verify trace output content across runtime category toggle phases.

Reads a Chrome Trace JSON file produced by the content-verification test
program and asserts that the correct events appear (or don't appear) in
each phase.

Phases (separated by categories_changed markers):
  1. ActorBehaviorTracing enabled       -> actor_behavior events present
  2. set to SchedulerStealingTracing    -> actor_behavior absent, steal events present
  3. set to empty                       -> both absent
  4. re-enable ActorBehaviorTracing     -> actor_behavior events present

A settle margin (microseconds) is skipped after each categories_changed
marker to account for multi-scheduler broadcast propagation.  Events in
the margin are ignored.
"""

import json
import sys

SETTLE_MARGIN_US = 500  # 500us — broadcast completes in microseconds

def load_events(path):
    with open(path) as f:
        text = f.read().rstrip().rstrip(",").rstrip()
        if not text.endswith("]"):
            text += "\n]"
        return json.loads(text)


def is_actor_behavior(ev):
    name = ev.get("name", "")
    return name in ("actor_behavior_run", "actor_behavior_schedule")


def is_steal_event(ev):
    name = ev.get("name", "")
    return name in ("steal_successful", "steal_failure_round")


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <ponytrace.json>", file=sys.stderr)
        sys.exit(1)

    events = load_events(sys.argv[1])

    # Find categories_changed markers in order
    markers = [ev for ev in events if ev.get("name") == "categories_changed"]
    if len(markers) < 4:
        print(f"FAIL: expected >= 4 categories_changed markers, got {len(markers)}")
        sys.exit(1)

    # categories_changed markers:
    #   [0]: enable actor_behavior
    #   [1]: set to scheduler_stealing (replaces actor_behavior)
    #   [2]: set to empty (disable all)
    #   [3]: re-enable actor_behavior
    #   [4]: cleanup disable
    #
    # Phases are the windows BETWEEN consecutive markers:
    #   Phase 1: markers[0]..markers[1] — actor_behavior active
    #   Phase 2: markers[1]..markers[2] — scheduler_stealing active
    #   Phase 3: markers[2]..markers[3] — nothing active
    #   Phase 4: markers[3]..markers[4] — actor_behavior active again
    if len(markers) < 5:
        print(f"FAIL: expected >= 5 categories_changed markers, got {len(markers)}")
        sys.exit(1)

    phase_bounds = [
        (markers[0]["ts"], markers[1]["ts"]),
        (markers[1]["ts"], markers[2]["ts"]),
        (markers[2]["ts"], markers[3]["ts"]),
        (markers[3]["ts"], markers[4]["ts"]),
    ]

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

    for phase_num, (start, end) in enumerate(phase_bounds, 1):
        settle_start = start + SETTLE_MARGIN_US
        settle_end = end - SETTLE_MARGIN_US

        if settle_start >= settle_end:
            print(f"FAIL: phase {phase_num} too short for settle margin "
                  f"(start={start}, end={end}, margin={SETTLE_MARGIN_US})")
            sys.exit(1)

        phase_events = [
            ev for ev in events
            if settle_start <= ev.get("ts", 0) <= settle_end
            and ev.get("name") != "categories_changed"
        ]

        ab_count = sum(1 for ev in phase_events if is_actor_behavior(ev))
        steal_count = sum(1 for ev in phase_events if is_steal_event(ev))

        print(f"Phase {phase_num} (settle window {settle_start}–{settle_end} us): "
              f"actor_behavior={ab_count}, steal={steal_count}")

        if phase_num == 1:
            check("phase 1: actor_behavior events present", ab_count > 0)
        elif phase_num == 2:
            check("phase 2: actor_behavior events absent", ab_count == 0)
            check("phase 2: steal events present", steal_count > 0)
        elif phase_num == 3:
            check("phase 3: actor_behavior events absent", ab_count == 0)
            check("phase 3: steal events absent", steal_count == 0)
        elif phase_num == 4:
            check("phase 4: actor_behavior events present", ab_count > 0)

    print(f"\n{pass_count} passed, {fail_count} failed")
    sys.exit(1 if fail_count > 0 else 0)


if __name__ == "__main__":
    main()
