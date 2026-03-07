## Fix non-sendable data seen as sendable in generic recover blocks

The compiler incorrectly allowed accessing generic fields inside `recover` blocks when the type parameter had an unconstrained capability or used `#alias`. For example, this unsound code was accepted without error:

```pony
class Foo[T]
  let _t: T
  new create(t': T) => _t = consume t'
  fun box get(): T^ =>
    recover _t end
```

When `T` is unconstrained (`#any`), concrete instantiations like `Foo[String ref]` would allow recovering a `ref` field to `val` — violating reference capability safety. The compiler now correctly rejects this with "can't access non-sendable field of non-sendable object inside of a recover expression".

Fields constrained to `#send` or `#share` are unaffected — all their concrete instantiations are sendable, so access inside `recover` remains valid.
