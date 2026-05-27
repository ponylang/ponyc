## Fix incorrect `#any` capability for type parameters with union constraints

A type parameter constrained by a union type — for example,
`[O: (Foo tag | Bar ref)]` — was being given the `#any` capability even when
every member's capability was in `#alias` (one of `ref`, `val`, `box`, or
`tag`). That made common patterns fail to type-check:

```pony
primitive Writer[O: (Async tag | Sync ref)]
  fun _write(out: O, data: String) =>
    None

  fun ok(out: O) =>
    _write(out, "+OK\r\n")  // error: O #any ! is not a subtype of O #any
```

The compiler now derives the correct capability for these constraints. In the
example above, the capability is `#alias` (the smallest capability set that
contains both `tag` and `ref`), and the program compiles.

The workaround of intersecting the union with `Any #alias` is no longer
needed.
