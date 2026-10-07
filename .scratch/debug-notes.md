# DCD Destruction — Agreed Design & Working Notes

## The agreed protocol

After chained confirmation succeeds (cycle detected, delegation/merge handled):

### Phase 1: CONF/ACK (two rounds) — prove quiescence

Round 1: Leader sends CONF_DCD to each member (direct queue push). Each
member checks: queue empty, rc == appearances, no ASIO events. Sends
ACK_DCD to leader. Leader collects with bitmask. Any NACK: reset.

Round 2: Same check again. If all ACK: cycle is proven quiescent.

Why two rounds: between rounds, if any member processed a message, it
generated new messages to other members (closed system). Round 2 catches
them (queue not empty -> NACK).

### Phase 2: Destroy — the classic CD protocol

Leader directly destroys all members from its own thread. No DESTROY
messages — members are sleeping, don't wake them up.

1. Leader calls ponyint_actor_setpendingdestroy on ALL members
2. For each non-leader member: final, sendrelease, destroy
3. Leader marks itself released; its own run loop handles self-destruction
   via the DCD release path

This is safe IF AND ONLY IF all invariants hold (see below).

sendrelease safety: all cycle members have pendingdestroy set.
send_release checks pendingdestroy on the target and skips (frees the
actorref without sending RELEASE). Non-cycle actors get RELEASE normally.

## Key invariants

If it crashes, either the code is wrong or an invariant is violated.

1. **Members are truly sleeping when leader destroys them.** CONF/ACK
   proved quiescence. No member is mid-run-loop. No thread is accessing
   member state. This means:
   - Cross-thread setpendingdestroy is safe (no concurrent writer on
     sync_flags — the member is sleeping)
   - sendrelease is safe (no thread iterating the member's foreign map)
   - The spin-wait in ponyint_actor_destroy is zero iterations (member
     already called markempty)

2. **set_sync_flag is NOT thread-safe in general.** It's a load-then-store,
   not an atomic RMW. Two threads writing different flags concurrently
   lose one. Only safe when the actor is sleeping (no concurrent writer).

3. **Self-reap is disabled** (currently returns false unconditionally).
   Unsafe once DCD has started because a leader may hold a raw pointer.

4. **DCD actors skip the forced try_gc at line ~828.** The forced GC
   (pony_triggergc + try_gc for blocked actors with rc > 0) exists to
   release references so the classic CD can see rc drop to 0. DCD detects
   cycles through trace routes, not rc drops. The condition was changed:
   `if (actor_noblock || (!actor_distributedcd && !BLOCKED_SENT))`

## Current state of the code

- distcd.c: Leader's handle_ack, after round 2 completes, calls
  setpendingdestroy on all members, then final+sendrelease+destroy on
  each non-leader member. Sets released=true for the leader.
- actor.c: DCD release path (line ~737) handles leader self-destruction:
  final, sendrelease, destroy when released and queue empty.
- No DESTROY messages. No send_destroy_dcd function. No handle_destroy.
- Self-reap returns false unconditionally.
- Forced try_gc at line ~828 skipped for DCD actors.

## Diagnostic approach

If it crashes: find which invariant is violated. Don't patch the
destruction code. The protocol is correct if the invariants hold.

Likely suspects:
- A member isn't actually sleeping when leader destroys it (CONF/ACK
  didn't prove quiescence correctly)
- A member woke up between confirmation and destruction
- Something outside the cycle is accessing a cycle member

## Build and test

- Build: cmake --build --preset debug
- Compile repro: cd build/debug && ./ponyc -b minimal-repro --checktree --pic ../../.scratch/minimal-repro
- Run: ./minimal-repro --ponydistributedcd --ponymaxthreads 4
- Stability: run 50 times, check for crashes

## Diagnosis session

### Finding 1: Two leaders destroying simultaneously

Backtrace shows TWO threads stuck spinning in ponyint_actor_destroy
inside handle_ack. A third thread is in handle_ack at a different
address (likely crashed). This means multiple actors both believe they
are the leader and both entered the destroy path for overlapping cycles.

The invariant violated: only one leader should destroy a given set of
members. Either:
- Two actors independently detected the same cycle and both started
  CONF/ACK
- A delegation/merge didn't prevent duplicate leadership
- The same leader processed two ACK completions (double-fire)

Next: check how try_confirm and the CONF/ACK state machine prevent
duplicate leadership. Check if handle_ack can fire the destroy path
more than once.

### Finding 2: CONF/ACK proves "was blocked" not "is sleeping"

The spin-wait in ponyint_actor_destroy hangs because a member hasn't
called markempty yet. The member processed CONF round 2, sent ACK,
but is still in its run loop when the leader processes the ACK and
calls destroy.

The CONF/ACK protocol proves the member was blocked at the instant it
checked. But between sending ACK and the leader processing it, the
member is still running (try_gc, try_confirm, markempty). The leader
can reach the destroy path before the member finishes.

With 1000 rings and 4 threads, many cycles are destroyed concurrently.
Trace routes from other cycles can arrive in a member's queue between
the ACK and markempty, causing the member to get rescheduled instead
of sleeping. The spin-wait then hangs forever.

This is the same race the classic CD has — but the classic CD avoids
it because it runs during scheduler quiescence when ALL actors have
finished their run loops. The DCD leader is a regular actor and
doesn't have that guarantee.

### Question for Sean

How does the classic CD ensure members are truly sleeping (not just
"confirmed blocked") before destroying them? Is there a scheduler
mechanism I should be using?

### Finding 3: Crash requires multiple concurrent cycles

1 ring (3 actors): 20/20 clean
10 rings: 10/10 clean
50 rings: 10/10 clean
100 rings: 7/10 crashed

Single cycle works. The bug is in how multiple cycles interact when
destroyed concurrently on different scheduler threads.

### Finding 4: 5 rings is the minimal crash shape

2 rings: 0/30 clean
3 rings: 0/30 clean
5 rings: 1/30 crashed

### Hypothesis (Sean's)

The cycle detection is detecting a partial or incorrectly merged cycle.
Members from different rings are being combined into one candidate, or
a subset of a ring is being confirmed. The leader tries to destroy what
it thinks is a complete cycle, but members outside the detected "cycle"
still hold references into it. The system isn't quiescent because the
invariant (rc == appearances) is wrong — the appearances count itself
is incorrect.

Needs proof: add debug output to print what the leader thinks the cycle
is at destruction time, verify whether the members form a real complete
cycle.

### Finding 5: Queue not empty at destruction time

Debug output shows a member with q=NOTEMPTY at destruction time. The
member ACKed "queue empty" during CONF round 2, but something arrived
between the ACK and the leader's destroy path.

All cycles are count=3, rc=1, app=1 — shapes look correct. No
duplicate members. The bug is a message arriving in a member's queue
after it confirmed quiescence.

Since rc==appearances (no external refs), the message must come from
within the cycle or from the protocol itself (CONF/ACK messages).

Next: check whether CONF or ACK messages are being pushed into
member queues after the final ACK.

### Finding 6: Fundamental soundness bug in CONF/ACK

**The bug**: The leader computes appearances for each member using
`count_appearances_in_component` on its OWN known_cycles, then sends
those numbers to each member. The member just checks
`my_rc == leader_sent_number`. The member has no way to verify the
leader's count is correct. If the leader's cycle knowledge is wrong
or incomplete, the member blindly trusts it.

**The fix (Sean's design)**: Each member must independently verify the
cycle is complete using its OWN data. When a member receives CONF with
the candidate member list, it:

1. Uses its own known_cycles to count how many cycle records contain
   itself AND are fully within the candidate set (same logic as
   count_appearances_in_component but using the member's own data)
2. Checks: my_rc == my_own_computed_appearances
3. If they don't match, NACK — the cycle isn't complete from this
   member's perspective

**Why this is sound**: rc is ground truth (maintained by the runtime).
If any member has an inbound reference from outside the candidate,
its rc will be higher than what the candidate's cycles account for,
and it will NACK. The key insight: we know we have a cycle, the
question is whether it's COMPLETE. If a member can't see its full rc
accounted for within the cycle, the cycle isn't safe to collect.

**Changes needed**:
- handle_conf: compute appearances locally instead of using leader's
- send_conf_dcd: stop sending appearances array (or ignore it)
- start_conf_round: leader also checks using its own appearances
  (it already does, from its own known_cycles — keep this)
- conf_dcd_msg_t: can remove appearances field

The leader still checks itself in start_conf_round using its own
count_appearances_in_component — that's already correct for the
leader. The fix is making members do the same.
