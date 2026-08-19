"""
Generative runtime stress harness.

A swarm-driven Pony program that exercises the runtime's message
passing, ORCA tracing, the cycle detector's collection path, the
explicit backpressure / muting path, ORCA's transfer of ownership
of a nested mutable object subgraph between actors, and ORCA's
actor-reference counting. One run draws one of five closed,
count-driven workloads (`--workload`): `mesh`, `cyclic`,
`backpressure`, `iso`, and `actorref`.

The workloads are closed: a fixed amount of work is injected and
the run terminates when it drains. All randomness is seeded only
from `--seed` (never the clock). Each workload's
driver/collector folds its scheduler-visible arrivals into an
FNV-1a `ORDER_SIG`, a pure function of the scheduler interleaving
for the determinism oracle.

Two orchestrators drive this program:
`orchestrate_systematic.py` (serialized, reproducible) and
`orchestrate_normal.py` (real-parallel runtime).
"""
