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
  wired in by the driver) so the still-open cycle-detector send ordering (#5569)
  can't confound it. When added it diverged per run against the pre-#5568 runtime
  at every thread count tried (2 through 16), so unlike `mute-order-signature` it
  does not depend on load/core-count balance to flake. Same driver and replay
  property.
