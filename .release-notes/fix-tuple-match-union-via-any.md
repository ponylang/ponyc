## Fix segfault when matching tuple elements against unions or interfaces via Any

Matching a tuple element against a union or interface type when the tuple was accessed through `Any val` caused a segfault at runtime:

```pony
actor Main
  new create(env: Env) =>
    let x: Any val = ("a", U8(1))
    match x
    | (let a: String, let b: (U8 | U16)) =>
      env.out.print(b.string())  // segfault
    end
```

The same crash happened when matching against an interface like `Stringable` instead of a union. Matching against concrete types (e.g., `let b: U8`) was unaffected.

This has been fixed. Tuple elements that are unboxed primitives are now correctly boxed when the match pattern requires a pointer representation.
