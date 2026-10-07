# Protocol Update Working Notes

## Completed
1. Gossip removed (25aee8e42)
2. Trace forwarding fixed (575e164d9, prior commit)
3. Confirmation protocol fixed for no-gossip (d63656eae) — leader pre-computes appearances
4. Delegate to actual denier (525c8b22e) — sender field in confirm_msg_t

## In Progress: Chained Confirmation

### Problem
Leader sends CONFIRM_BLOCKED directly to all members. But the leader only has direct connections (foreign map entries) to SOME members. Sending to actors not in the leader's foreign map is a use-after-free risk.

### Solution
Chain CONFIRM_BLOCKED through the cycle topology. Each hop is to an actor in the sender's foreign map. The message accumulates confirmations as it travels.

### New message types
- confirm_chain_msg_t: members[], appearances[], count, leader, confirmed_bits, denied, denier
- delegate_chain_msg_t: members[], count, denier, visited_bits
- release_chain_msg_t: members[], appearances[], count, leader

### Key design decisions (verified with boss)
- confirmed_bits is uint64_t — assert at 64 members (real components are single-digit)
- appearances[] is a parallel array, copied with each message clone
- Leader checks m->leader == actor to distinguish chain return from member processing
- DELEGATE carries members[] for routing + denier identity, no appearances needed
- No fork counting for chain returns — leader just unions confirmed_bits until all set
- CONFIRMED and DENIED message types eliminated — folded into chain

### Implementation plan
1. distcd.h: new message types, update candidate_record_t
2. actor.h: remove CONFIRMED/DENIED message IDs
3. actor.c: remove CONFIRMED/DENIED dispatch, update types for other dispatch cases
4. distcd.c: new send helpers, rewrite try_confirm/handle_confirm_blocked/handle_delegate/handle_release, delete handle_confirmed/handle_denied

## Decision Log
1. Trace forwarding already fixed in prior commit
2. DENIED handling already simple
3. Keep component/leader infrastructure
4. Pre-computed appearances in messages (verified with boss)
5. Delegate to actual denier via sender field
6. Chained confirmation to avoid use-after-free from broadcasting to non-foreign-map actors
