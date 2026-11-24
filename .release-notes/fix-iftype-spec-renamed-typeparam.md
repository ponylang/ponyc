## Fix `iftype` specialization selected for non-matching types when parameter names differ

An `iftype` specialization could run for a type that fails its guard when the specialization and the default method used different names for their type parameters.

```pony
primitive Example
  fun check[A: Any val](): I32 => 10
  fun check[B: Any val](): I32 iftype B <: U8 => 30
```

Calling `Example.check[U16]()` returned `30` instead of `10`. The guard `B <: U8` should fail for `U16`, selecting the default body.
