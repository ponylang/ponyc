## Fix exhaustive match on tuples of union types

When using `match \exhaustive\` on a tuple of union types, the compiler rejected matches that used concrete types for the final case even when they covered all remaining possibilities. For example, this was incorrectly rejected:

```pony
type A is String

actor Main
  new create(env: Env) =>
    let x: (A | None) = None
    let y: (A | None) = None

    match \exhaustive\ (x, y)
    | (let a: A, _) => env.out.print("1")
    | (_, let a: A) => env.out.print("2")
    | (None, None) => env.out.print("3")
    end
```

The compiler reported `match marked \exhaustive\ is not exhaustive`, even though the first two cases cover every combination where at least one element is `A`, and the third case covers the only remaining possibility `(None, None)`. Replacing `(None, None)` with `(_, _)` was accepted as a workaround.

The underlying issue was in the subtype checker: a tuple of unions like `((A | None), (A | None))` was not recognized as a subtype of the equivalent union of tuples. This is now fixed by distributing tuples over unions when checking subtype relationships.
