## Fix soundness bug with aliased type parameter constraints

When a type parameter's constraint used `!` on another type parameter, the compiler ignored the `!` modifier. This was a soundness hole: the compiler accepted code that violated reference capability guarantees.

The `!` modifier aliases a capability — `iso` becomes `tag`, `trn` becomes `box`. In this example, X should be constrained to `A tag` (because `Y!` where `Y: A iso` means "the aliased form of `A iso`," which is `A tag`). Before this fix, the compiler treated X as `A iso`, allowing code like this to compile:

```pony
class A
  var data: String = "hello"

actor Main
  new create(env: Env) =>
    let opaque: A tag = A
    // This should not compile — opaque is tag, not iso
    let stolen: A iso = reveal[A tag, A iso](opaque)

  fun reveal[X: Y!, Y: A iso](x: X): A iso^ =>
    consume x
```

The compiler now correctly rejects this code because `consume x` produces `A tag^`, not `A iso^`.

If your code stops compiling after this fix, look for type parameters constrained with `!` through another type parameter — the `!` is now enforced, so the constraint is tighter than it was before. The compiler error will show the actual capability. Adjust your code to work with the aliased capability (e.g. `tag` instead of `iso`, `box` instead of `trn`).
