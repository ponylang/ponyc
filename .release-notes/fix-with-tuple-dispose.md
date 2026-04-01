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
