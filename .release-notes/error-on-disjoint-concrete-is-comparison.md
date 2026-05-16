## Reject identity comparison between distinct concrete types

The compiler now rejects `is` and `isnt` comparisons whose operand types are two distinct concrete entity types (any pair drawn from `class`, `actor`, `primitive`, and `struct`). Such comparisons can never be true, so they are almost always a bug; flagging them at compile time surfaces the mistake immediately instead of producing a constant `false` at runtime.

```pony
class C
class D

actor Main
  new create(env: Env) =>
    let c: C = C
    let d: D = D
    if c is d then None end  // now a compile-time error
```

The rule fires only when both operands resolve to a single concrete nominal type. Comparisons involving traits, interfaces, unions, intersections, tuples, or type parameters continue to compile, even when one side is concrete. Two reifications of the same generic class (for example `Array[U64]` and `Array[U32]`) share a single root definition and are also not flagged. Two `object end` literals at different source locations synthesize distinct anonymous classes and are flagged.

If you hit the new error, the comparison was unreachable. Switch to structural equality (`==`) or remove the comparison.
