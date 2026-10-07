# Forward-to-one fix — working notes

## Task
Replace the loop in handle_trace_route (lines 828–836) that forwards to ALL
outgoing connections with a single forward to one connection.

## Plan
1. Edit distcd.c: replace the while loop with a single ponyint_actormap_next + dedup + send
2. Build debug
3. Run ci-core tests
4. Review loop (principle-review)
5. Code review
6. Squash and open PR

## Decision log
(Autonomous mode — decisions recorded here)

### D1: Which connection to forward to
- Options: first-in-iteration, hash-of-originator, random
- Call: First-in-iteration (option 1 from the handoff). Simplest, no extra state.
  The handoff proposes this and notes all options are identical for F=1 (rings).
  For F>1, the correctness note explains traces still find all cycles over time.
  No reason to add complexity for a case the protocol handles via retrace.

## Status
- [x] Edit made
- [x] Build passes
- [x] Tests pass (runtime + compiler unit tests pass; full-program/stdlib/grammar
  failures are sandbox permission issues — "build process failed to start" /
  "exit permission denied" — not code failures)
- [ ] Review done (principle-review-1 running)
- [ ] PR opened
