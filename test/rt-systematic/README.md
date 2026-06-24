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
