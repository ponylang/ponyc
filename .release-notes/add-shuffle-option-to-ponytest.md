## Add --shuffle option to PonyTest

PonyTest now has a `--shuffle` option that randomizes the order tests are dispatched. This catches a class of bug that's invisible under fixed ordering: test B passes, but only because test A ran first and left behind some state. You won't find out until someone removes test A and something breaks in a way that's hard to trace.

Use `--shuffle` for a random seed or `--shuffle=SEED` with a specific U64 seed for reproducibility. When shuffle is active, the seed is printed before any test output:

```
Test seed: 8675309
```

Grab that seed from your CI log and pass it back to reproduce the exact ordering:

```
./my-tests --shuffle=8675309
```

Shuffle applies to all scheduling modes. For CI environments that run tests sequentially to avoid resource contention, `--sequential --shuffle` is the recommended combination: stable runs without flakiness, and each run uses a different seed so test coupling surfaces over time instead of hiding forever.

`--list --shuffle=SEED` shows the test names in the order that seed would produce, so you can preview orderings without running anything.
