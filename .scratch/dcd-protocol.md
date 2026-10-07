# Distributed Cycle Detection Protocol

Collect garbage cycles of blocked actors without a central cycle
detector. This protocol replaces the classic cycle detector
entirely.

## Per-actor state (`distcd_t`)

- `epoch: uint32` — incremented on outgoing connection loss or rc
  change. Stale traces (carrying an old epoch for this actor) are
  discarded on receipt.
- `known_cycles: list<cycle_record>` — each record is a sorted
  array of actor pointers. Deduplicated: a new record is rejected
  if it equals or is a subset of an existing record.
- `retrace_needed: bool` — set on epoch bump. Cleared when the
  actor blocks and re-traces.
- `trace_dedup: map<(target, originator, epoch), bool>` — prevents
  forwarding the same trace twice.
- `candidate: candidate_record` — non-null only on the leader,
  only during phases 2–3.
- `conf_state: enum {NONE, COMPLETENESS_WAITING, CONF_ROUND1,
  CONF_ROUND2}` — tracks protocol progress on the leader.
- `destroy_ready: bool` — set when all round 2 ACKs received.
  Destruction deferred to next `on_block`.
- `confirmed_idle: bool` — set by non-leader members after ACKing
  CONF round 2. Prevents re-running try_confirm.

Self-reap is disabled for all actors with DCD state. Actor
pointers in cycle records and candidates cannot become dangling.

## Integration with the actor run loop

DCD hooks into the actor run loop at the **blocking path** — the
code that runs when an actor has drained its message queue and is
about to sleep.

```
ponyint_actor_run(ctx, actor):
  ... drain messages ...
  if BLOCKED:
    if actor->distcd != NULL:
      if distcd_on_block(ctx, actor, dcd):
        // Destruction happened. Leader self-destructs here.
        ponyint_messageq_markempty(&actor->q)
        ponyint_actor_destroy(actor)
        return false   // do not reschedule
      // else: protocol ran but no destruction yet
      // fall through to markempty + return
    else:
      ... normal blocking/self-reap path ...
```

`distcd_on_block` returns true ONLY when the leader has completed
destruction of all non-leader members and is ready to self-destruct.
When it returns true, the caller does ONLY: markempty, destroy,
return false. Nothing else — no GC, no try_confirm, no block
teardown.

`distcd_on_block` returns false in all other cases. The actor
proceeds to markempty and returns normally.

## Phase 1: Discovery

**Trigger.** An actor blocks and `retrace_needed` is true.
`retrace_needed` starts true (initial trace) and is set on every
epoch bump.

**Action.** The actor sends TRACE_ROUTE to every actor in its
foreign map. The message carries an array of `(actor*, epoch)`
entries — the path so far, starting with `(self, self.epoch)`.

**On receiving TRACE_ROUTE.** Let `entries` be the path in the
message.

1. **Self appears in `entries` with matching epoch.** Cycle found.
   Members = actors from self's position in `entries` through the
   end. Sort by pointer. Add to `known_cycles` if no existing
   record equals it or is a superset of it.

2. **Self appears in `entries` with non-matching epoch.** Stale.
   Discard. Free `entries`.

3. **Self does not appear.** Append `(self, self.epoch)` to
   `entries`. Pick one outgoing connection: hash the originator
   (first entry's actor) to select a target from the foreign map.
   Check `trace_dedup` for `(target, originator, originator_epoch)`.
   Not found: record it, forward the message. Found: discard.

4. **Self does not appear, no outgoing connections.** Buffer in
   `pending_traces`. Process on next `on_acquire`.

**Result.** Every actor in a cycle holds a record of that cycle.
Different actors may hold different overlapping records.

**Invariant established.** Each actor's `known_cycles` contains
cycle records that describe real reference cycles — paths where
following outgoing references leads back to the originator. The
records are sorted arrays of actor pointers, deduplicated.

## Phase 2: Completeness verification

**Purpose.** Verify that the proposed cycle is COMPLETE — every
inbound reference to every member comes from within the proposed
set. If any member has an inbound reference from outside the set,
the cycle is incomplete and must not be collected.

**Trigger.** `try_confirm` runs each time a blocked actor reaches
the blocking path. The actor:

1. Computes a component: transitive closure of overlapping cycle
   records from `known_cycles`. Two records overlap if they share
   a member. The component is the union of all members across
   overlapping records.
2. Picks a leader: the actor in the component with the most
   appearances across the component's cycle records. Ties broken
   by lowest pointer address.
3. If self is not the leader: return.
4. If `conf_state != NONE`: return (already in progress).

**The completeness check.** Every member — leader included —
performs the same check:

1. Count how many of the component's cycle records contain self.
   Call this `own_app`. Each record that contains self represents
   one inbound reference path to self within the cycle.
2. Compare `own_app` against `self.gc.rc`. `rc` is the actual
   number of actors that hold a reference to self (ground truth,
   maintained by the runtime via ACQUIRE/RELEASE messages).
3. `self.gc.rc == own_app`: complete. Every inbound reference to
   self is accounted for by the cycle records.
4. `self.gc.rc != own_app`: incomplete. Self has inbound
   references not accounted for by the cycle records.

The leader performs this check on itself FIRST, using the
component records it just computed. If the leader's own check
fails (`self.gc.rc != own_app`): return without sending any
messages. The cycle is incomplete.

If the leader's own check passes, it sends the component's
cycle records to each non-leader member so they can perform
the same check independently. The member receives the actual
cycle records — not a pre-computed count, not just a member
list. The member counts its appearances in THESE RECORDS and
compares against its own `rc`.

The member does NOT use its own `known_cycles`. It uses the
records the leader sent. `rc` is the ground truth.

**Send COMPLETENESS_CHECK.** Push directly into the member's
queue via `ponyint_actor_messageq_push`. Set
`conf_state = COMPLETENESS_WAITING`.

Message payload:
```
members:       actor*[]        — the component member list (sorted)
count:         size_t          — number of members
cycle_records: cycle_record[]  — the cycle records that form
               the component (each is a sorted array of actor
               pointers representing one detected cycle)
num_records:   size_t          — number of cycle records
leader:        actor*          — the leader sending this check
member_index:  size_t          — receiver's index in members[]
```

**On receiving COMPLETENESS_CHECK.** The member performs the
completeness check described above using the cycle records from
the message. ACK if complete, NACK if not.

Response payload:
```
leader:       actor*     — destination
member_index: size_t     — which member responded
ack:          bool       — true = complete, false = incomplete
```

**Leader collects responses.** Track in a bitmask on `candidate`.

- All members ACK: proceed to phase 3.
- Any NACK: set `conf_state = NONE`. Free candidate. Remove
  the component's cycle records from `known_cycles`. Stop.

**After denial.** The cycle records are removed from
`known_cycles`. The leader will not re-propose this cycle. When
the topology later changes (the external reference drops), the
affected member's rc changes, which bumps its epoch, which sets
`retrace_needed`, which causes retrace on next block. The
retrace re-discovers the cycle from scratch.

**Invariant established (all members ACK).** For every member
in the candidate:

- `rc == appearances_in_cycle_records` — every inbound reference
  to this member comes from within the candidate set.
- NO actor outside the candidate holds a reference to ANY member.
- Therefore: no outside actor can send ANY message to any member.
  Not application messages, not trace routes, not anything. The
  only messages members can receive are from each other.

This invariant is critical for phase 3: it guarantees that once
quiescence is confirmed, it cannot be broken by external actors.

## Phase 3: Quiescence confirmation (CONF/ACK)

**Purpose.** Prove all members are blocked with empty queues and
no pending I/O. Two rounds to catch messages generated by
processing round 1.

**Leader self-check (before each round).** The leader checks:

- `ponyint_messageq_isempty(&self->q)` — false: set
  `conf_state = NONE`, free candidate, stop.
- `self->live_asio_events == 0` — false: same.

**Send CONF to each non-leader member.** Push directly into the
member's queue.

Message payload:
```
members: actor*[]  — the candidate member list
count:   size_t    — number of members
leader:  actor*    — this actor
round:   uint32    — 1 or 2
```

**On receiving CONF.** The member checks:

- `ponyint_messageq_isempty(&self->q)` — is the queue empty NOW,
  after consuming the CONF message.
- `self->live_asio_events == 0`.

Both true: push ACK to leader. Either false: push NACK to leader.

No rc check. Completeness was verified in phase 2.

If the member ACKs round 2, set `confirmed_idle = true`. This
prevents the member from running `try_confirm` again (it has
confirmed quiescence and should do nothing further).

Response payload:
```
leader:       actor*     — destination
member_index: size_t     — which member responded
round:        uint32     — which round this is for
ack:          bool       — true = blocked, false = not blocked
```

**Leader collects.**

- All ACK round 1: set `conf_state = CONF_ROUND2`. Run leader
  self-check again. Send CONF round 2.
- All ACK round 2: set `destroy_ready = true`. Destruction is
  deferred (see below).
- Any NACK: set `conf_state = NONE`. Free candidate. Stop.

**Why two rounds.** Processing a CONF message is work — the
member was scheduled and ran. If processing generated a message
to another member (it shouldn't in a blocked cycle, but the
protocol must not rely on application-level reasoning), round 2
catches it: the recipient's queue is no longer empty.

**Why destruction is deferred.** When the leader receives the
last round 2 ACK, it is inside its message-processing loop (it
just processed the ACK message). It must not destroy members
from inside the message loop. Instead, it sets `destroy_ready =
true`. Destruction happens when `distcd_on_block` runs — the
blocking path at the end of the run loop, after all messages
have been drained.

**Invariant established (all round 2 ACKs received).** After
all round 2 ACKs:

- Every member's queue is empty.
- No member has pending ASIO events.
- No actor outside the candidate can send messages to any member
  (from the completeness invariant).
- Therefore: no new messages can arrive in any member's queue.
- Therefore: every non-leader member has finished its run loop
  and called `markempty` on its queue. It is sleeping — not
  scheduled, not running on any thread.
- Therefore: it is safe for the leader to access non-leader
  member state directly — no concurrent readers or writers.

The leader itself is still running (it's in its run loop). It
will reach the blocking path shortly and enter `distcd_on_block`,
where it finds `destroy_ready == true` and performs destruction.

## Phase 4: Destruction

**Trigger.** `distcd_on_block` runs and `destroy_ready` is true.

**Context.** This code runs inside the leader's run loop, in the
blocking path, on the leader's scheduler thread. All non-leader
members are sleeping (proven by the phase 3 invariants).

**Implementation.** All of steps 1–4 below happen inside a single
function (`do_destruction`). Steps 1–4 are SEPARATE PASSES over
the member list — not per-member. This ordering is critical:

**Step 1: setpendingdestroy.** Call `ponyint_actor_setpendingdestroy`
on every member including the leader. This sets a flag that
`sendrelease` checks on targets.

Safe because: non-leader members are sleeping (no concurrent
writers to `sync_flags`). The leader is the only thread touching
these actors.

**Step 2: final.** Call `ponyint_actor_final` on every member
including the leader. This runs Pony finalizers.

Must happen before sendrelease: a finalizer may modify the
actor's state (drop references, clean up resources). sendrelease
must process the post-finalization foreign map.

Safe because: same as step 1 — no concurrent access.

**Step 3: sendrelease.** Call `ponyint_actor_sendrelease` on every
member including the leader.

For each entry in a member's foreign map, sendrelease checks the
target's `pendingdestroy` flag:
- Target has `pendingdestroy` set (it's a fellow cycle member):
  free the actorref locally. Do NOT send a RELEASE message.
- Target does NOT have `pendingdestroy` set (it's outside the
  cycle): send a RELEASE message to the target.

Must happen before destroy: sendrelease reads the foreign map of
each member, and reads `pendingdestroy` on targets. All member
memory must still be valid.

Note: because the completeness invariant proved no outside actor
holds a reference to any member, sendrelease will only SEND
RELEASE messages to actors that members reference outward (not
inward to the cycle). This is correct — those outward references
need to be cleaned up.

**Step 4: destroy non-leaders.** Call `ponyint_actor_destroy` on
every non-leader member.

`ponyint_actor_destroy` spin-waits for the queue's markempty bit.
Non-leader members are sleeping — they already called markempty
at the end of their last run loop (proven by phase 3 invariants).
The spin-wait is zero iterations.

Free each non-leader member's `distcd_t` before destroying the
actor (destroy frees the actor struct itself).

**After `do_destruction` returns** (back in the run loop):

`distcd_on_block` returned true. The run loop code does:

1. `ponyint_messageq_markempty(&self->q)` — the leader is still
   running, so its queue hasn't been marked empty yet. destroy
   spin-waits for this bit; without it, destroy hangs.
2. Free the leader's own `distcd_t`.
3. `ponyint_actor_destroy(self)`.
4. Return false from the run loop. Do not reschedule.

Nothing else executes after this — no GC, no try_confirm, no
block teardown. The leader's heap, foreign map, and known_cycles
all contained references to the now-freed members. Any code that
touches those data structures after destruction would crash.

**Invariant established.** All cycle members are freed. Outward
references are cleaned up via RELEASE messages. The scheduler
will not reschedule any destroyed actor.

## Topology change handling

### Epoch bump

Two variants — **full** and **light** — depending on whether
the topology change can invalidate existing cycle records.

**Full epoch bump** — bump `epoch`, set `retrace_needed = true`,
clear ALL `known_cycles`, on:

1. **ACQUIRE received** — rc increase means a new inbound
   reference. Existing cycle records may now be incomplete.

2. **Outgoing connection loss** — GC sweep removes a foreign
   reference. Additionally: remove dedup entries for the lost
   target from `trace_dedup`.

Clearing `known_cycles` on full bump is critical for safety.
When a cycle is destroyed, `sendrelease` sends RELEASE messages
to outside actors. Those actors' rc changes, triggering an
epoch bump. If the old cycle records survive the bump, they
hold dangling pointers to freed actors — dereferencing them
during `try_confirm` crashes. Clearing all records eliminates
the dangling pointers. Retrace rediscovers valid cycles from
scratch.

**Light epoch bump** — bump `epoch`, set `retrace_needed = true`,
keep `known_cycles`, on:

1. **RELEASE received** — rc decrease. A RELEASE can only make
   a cycle MORE complete (fewer outside references), never less.
   Existing cycle records remain valid.

The split matters for throughput. Without it, a cycle whose
members receive RELEASE messages (common — the spawner that
created the ring drops its references via GC sweep) clears its
known_cycles on every RELEASE, forcing perpetual rediscovery.
The light bump lets cycles that are already known proceed
directly to completeness checking.

### Retrace

On next block after epoch bump, the actor re-traces all outgoing
connections (same as phase 1). The new epoch passes through other
actors' dedup tables (keyed on `(target, originator, epoch)`), so
traces propagate even if previous traces from this originator were
already forwarded.

### Dedup cleanup

On full epoch bump, clear own `trace_dedup`. Light bumps leave
`trace_dedup` intact (the outgoing topology hasn't changed).
Other actors' stale entries (old epoch for this originator)
don't block new traces and are replaced lazily when a dedup
check finds a matching `(target, originator)` with a different
epoch.

## Runtime termination

DCD replaces cycle collection but not runtime termination. The
classic cycle detector performs both: it collects garbage cycles
AND drives shutdown via CNF/ACK with the scheduler when all
actors are quiescent.

Without a termination mechanism, programs that use DCD hang
after all work is done — non-cyclic actors (coordinators,
spawners, timers) with rc > 0 cannot self-reap and nothing
triggers shutdown.

**TODO**: Design the termination mechanism. Options under
consideration:

1. Keep the existing CD actor in DCD mode but disable its
   cycle-detection half. It only does termination signaling.

2. Move termination logic into the scheduler — detect when
   all actors are blocked/destroyed and trigger shutdown
   directly.

## What this protocol does not have

- **No chained messages.** All protocol messages are point-to-point
  between leader and member. The leader has all member pointers
  and pushes directly into their queues.

- **No delegation.** Denial means abandonment. The topology change
  mechanism handles re-triggering.

- **No periodic gossip.** Cycle knowledge spreads through
  event-driven trace routes.

- **No central cycle detector.** Each actor participates using only
  its own local state.
