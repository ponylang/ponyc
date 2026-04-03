## Fix use-after-free in IOCP ASIO system

We fixed a pair of use-after-free races in the Windows IOCP event system. A previous fix introduced a token mechanism to prevent IOCP callbacks from accessing freed events, but missed two windows where raw pointers could outlive the event they pointed to. One was between the callback and event destruction, the other between a queued message and event destruction.

This is the hard part that Pony protects you from. Concurrent access to mutable data across threads is genuinely difficult to get right, even when you have a mechanism designed specifically to handle it.

## Remove support for Alpine 3.20

Alpine 3.20 has reached end-of-life. We no longer test against it or build ponyc releases for it.

## Fix with tuple only processing first binding in build_with_dispose

When using a `with` block with a tuple pattern, only the first binding was processed for dispose-call generation and `_` validation. Later bindings were silently skipped, which meant dispose was never called on them and `_` in a later position was not rejected.

For example, the following code compiled without error even though `_` is not allowed in a `with` block:

```pony
class D
  new create() => None
  fun dispose() => None

actor Main
  new create(env: Env) =>
    with (a, _) = (D.create(), D.create()) do
      None
    end
```

This now correctly produces an error: `_ isn't allowed for a variable in a with block`.

Additionally, valid tuple patterns like `with (a, b) = (D.create(), D.create()) do ... end` now correctly generate dispose calls for all bindings, not just the first.

## Fix memory leak in Windows networking subsystem

Fixed a memory leak on Windows where an IOCP token's reference count was not decremented when a network send operation encountered backpressure. Over time, this could cause memory to grow unboundedly in programs with sustained network traffic.

## Remove docgen pass

We've removed ponyc's built-in documentation generation pass. The `--docs`, `-g`, and `--docs-public` command-line flags no longer exist, and `--pass docs` is no longer a valid compilation limit.

Use `pony-doc` instead. It shipped in 0.61.0 as the replacement and has been the recommended tool since then. If you were using `--docs-public`, `pony-doc` generates public-only documentation by default. If you were using `--docs` to include private types, use `pony-doc --include-private`.

## Fix spurious error when assigning to a field on an `as` cast in a try block

Assigning to a field on the result of an `as` expression inside a `try` block incorrectly produced an error about consumed identifiers:

```pony
class Wumpus
  var hunger: USize = 0

actor Main
  new create(env: Env) =>
    let a: (Wumpus | None) = Wumpus
    try
      (a as Wumpus).hunger = 1
    end
```

```
can't reassign to a consumed identifier in a try expression if there is a
partial call involved
```

The workaround was to use a `match` expression instead. This has been fixed. The `as` form now compiles correctly, including when chaining method calls before the field assignment (e.g., `(a as Wumpus).some_method().hunger = 1`).

## Fix segfault when using Generator.map with PonyCheck shrinking

Using `Generator.map` to transform values from one type to another would segfault during shrinking when a property test failed. For example, this program would crash:

```pony
let gen = recover val
  Generators.u32().map[String]({(n: U32): String^ => n.string()})
end
PonyCheck.for_all[String](gen, h)(
  {(sample: String, ph: PropertyHelper) =>
    ph.assert_true(sample.size() > 0)
  })?
```

The underlying compiler bug affected any code where a lambda appeared inside an object literal inside a generic method and was then passed to another generic method. The lambda's `apply` method was silently omitted from the vtable, causing a segfault when called at runtime.

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

