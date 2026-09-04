## Fix type argument constraint check rejecting capability subtypes

A type argument whose capability was a subtype of the constraint's capability was incorrectly rejected. For example, `Array[U8] val` as a type argument for a `ByteSeq box` constraint produced "type argument is outside its constraint" even though `val` is a subtype of `box`.

This most commonly surfaced through type argument inference when a consumed `iso` argument caused the inferred type to use `val` instead of the constraint's `box`.

```pony
primitive Checker
  fun apply[S: ByteSeq box = ByteSeq box](xs: S, ys: S): Bool => true

actor Main
  new create(env: Env) =>
    let a: Array[U8] val = [as U8: 1; 2; 3]
    var b: Array[U8] iso = recover iso [as U8: 4; 5; 6] end
    Checker(a, consume b) // previously: "type argument is outside its constraint"
```
