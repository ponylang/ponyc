# Runtime systematic-testing fixtures

Standalone Pony programs used to check the runtime's `use=systematic_testing`
behavior. They are not part of the normal test suites; they are compiled and run
by a dedicated CI script.

- `order-signature/` — eight senders fan messages into one collector, which
  folds the arrival order into an order-sensitive hash printed as `ORDER_SIG`.
  The signature is a pure function of the scheduler interleaving, so it is the
  observable used to check that a fixed `--ponysystematictestingseed` replays the
  same interleaving across runs (with `--ponynoscale`), while different seeds
  still produce different ones. Driven by
  `.ci-scripts/systematic-testing/determinism_smoke.py`, run weekly from
  `.github/workflows/ponyc-weekly-checks.yml`.
- `mute-order-signature/` — a ring of nodes forwards tokens with deterministic
  `(id + 1) % n` routing, then reports arrivals into the same `ORDER_SIG` hash.
  Unlike `order-signature`, the volume of foreign actor-to-actor sends overloads
  actors and exercises the muting/unmuting reschedule path, so it guards a
  different slice of the same replay property. Same driver, run at the runtime's
  default thread count; its node/token counts are sized so actors overload at the
  small physical-core counts a CI runner typically has (the muting-driven
  reordering only appears when thread count and load are balanced).
- `acquire-release-order-signature/` — a mesh of nodes forwards tokens like
  `mute-order-signature`, but every hop carries a freshly allocated `String val`
  payload and the route spreads (`(id + hops) % n`, not a ring). Because each
  fresh payload is owned by the node that forwards it, a receiving node holds
  references to `String`s owned by many distinct nodes, so the ORCA
  reference-counting sends (`ACTORMSG_ACQUIRE` on forward, `ACTORMSG_RELEASE` on
  GC sweep) go out one per distinct owner — the path #5568 makes
  layout-independent. It runs with the cycle detector disabled (`--ponynoblock`,
  wired in by the driver) so the cycle detector's own send ordering can't confound
  it. When added it diverged per run against the pre-#5568 runtime at every thread
  count tried (2 through 16), so unlike `mute-order-signature` it does not depend
  on load/core-count balance to flake. Same driver and replay property.
- `cycle-collection-order-signature/` — successive generations of Worker groups,
  each member holding the whole group's array so the group is one strongly
  connected reference cycle. The Workers forward chains of fresh `String val`
  payloads to random group members (folding each chain's terminal arrival and
  receive count into `ORDER_SIG`), then the group is dropped and becomes garbage
  the cycle detector reclaims. It runs with the detector ENABLED and swept
  frequently (`--ponycdinterval 10`, wired in by the driver), so it exercises
  every pointer-ordered detector path at once — the `ACTORMSG_ISBLOCKED` probes,
  the `ACTORMSG_CONF` confirmations, the deferred-detection order, and `collect`'s
  per-member release sends — each of which walked a `ponyint_hash_ptr(actor)` map
  in pointer order before #5569. Against the pre-#5569 runtime it diverged per run
  from a few cores up (ASLR-off and `--ponynoblock` both make it reproduce,
  isolating the cause to the detector); after the fix it replays one. Because the
  cycles are really reclaimed it also guards the collection path against a crash
  regression. Same driver and replay property; tuned to flake at the small
  physical-core counts a CI runner has.
