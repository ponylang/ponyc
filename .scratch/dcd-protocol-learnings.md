# DCD Protocol Learnings

## Cycle completeness must be verified independently by each member

The leader computing appearances from its own known_cycles and
sending those numbers to members is unsound. The member checks
`my_rc == leader_sent_number` — if the leader's count is wrong,
the member has no way to catch it.

Each member must independently verify the cycle is complete using
its own data: count how many cycle records in its own known_cycles
contain itself and are fully within the candidate set, compare
against its own rc. rc is ground truth (maintained by the runtime).
If a member can't account for all its inbound references within the
candidate, the cycle isn't complete and isn't safe to collect.

## Re-triggering after topology changes

### The problem

Scenario: A → B → C → A, with D → B. B has rc=2 but only 1
appearance within the cycle {A,B,C}. B rejects the cycle as
incomplete. Correct.

Then D drops its reference to B (GC sends RELEASE). B's rc drops
to 1. The cycle {A,B,C} is now complete. But nothing re-triggers
exploration or confirmation:

- B's epoch doesn't change — epoch only changed on outgoing
  connection loss, not on incoming reference drops.
- The leader is sleeping (no messages after the NACK). No gossip
  to wake it.
- B processes RELEASE and runs try_confirm, but if B isn't the
  leader, it returns early.

### After denial, cycle records are unreliable

After a cycle is denied, we can't assume the other cycle members
still exist (they might have been freed by another cycle's
collection or self-reaped). We also can't assume the cycle would
be denied again (the external reference that caused the denial
might be gone). Cycle records hold raw pointers to actors — if
those actors are freed, the pointers are dangling.

The old gossip protocol crashed on exactly this: sending gossip
to freed actors (commit f2c9a8de9).

### The solution: epoch bump on rc change

Bump epoch on BOTH rc change and connection lost. Set
retrace_needed on both. Retrace on next block.

When B's rc changes (D drops its reference), B bumps epoch. On
B's next block, B retraces to its outgoing connections. The trace
propagates: B → C → A → B. The new epoch clears the dedup at
each hop (dedup is keyed on (target, originator, epoch)). All
cycle members get woken by the trace route messages. Whoever the
leader is runs try_confirm and retries confirmation. B's rc now
matches — cycle confirms.

No gossip needed. The trace route propagation IS the re-trigger.

### Dedup table cleanup

When an actor bumps its epoch, stale dedup entries accumulate:

- The actor's own dedup table has entries referencing the old
  epoch — clear them on epoch bump.
- Other actors' dedup tables have entries with the old epoch for
  this originator — these don't block new traces (different epoch
  passes dedup) but waste memory. Prune lazily: when a dedup
  check encounters an entry for the same (target, originator) but
  a different epoch, replace it.

## The completeness check IS gossip

The "is this cycle complete" verification — where each member
checks the candidate list against its own knowledge — is itself
a form of gossip. The CONF message carries the candidate member
list to each member. The member uses that list plus its own
known_cycles to make an independent judgment. Information about
the cycle composition flows from leader to members and gets
verified against local knowledge.

Removing gossip entirely was too aggressive. The completeness
check requires exactly this kind of information exchange. But we
don't need periodic broadcast gossip — structured, event-driven
re-exploration (epoch bump + retrace on rc change) gives us the
re-triggering without the problems that led to gossip's removal
(amplification, use-after-free on freed actors, stale records).
