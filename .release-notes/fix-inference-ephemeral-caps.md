## Fix type argument inference for consumed arguments

When two arguments to a generic function gave different capabilities for the same type parameter, and one was an ephemeral subtype of the other, inference reported a conflict instead of picking the supertype. Consuming an `iso` or `trn` variable and passing it alongside a `val` argument would fail even though the consumed value can satisfy `val`.

```pony
primitive Checker
  fun apply[S: ByteSeq val](xs: S, ys: S): Bool => true

actor Main
  new create(env: Env) =>
    let a: Array[U8] val = [1; 2; 3]
    let b: Array[U8] iso = recover iso [as U8: 4; 5; 6] end
    Checker.apply(a, consume b) // previously: "conflicting types for type parameter 'S'"
```

Consumed `iso^` and `trn^` values are now treated as subtypes of `val` when resolving the type parameter, so the supertype is picked instead of raising a conflict. Writing explicit type arguments is no longer needed.
