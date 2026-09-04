## Add PonyCheck generators for persistent collections

PonyCheck now has generators for all persistent collection types: `Vec`, `List`, `Set`, `SetIs`, `Map`, and `MapIs` from `collections/persistent`. Each generator controls size with `from` and `to` parameters and supports shrinking.

```pony
use "collections/persistent"
use "pony_check"

// Vec
Generators.vec_of[U8](Generators.u8() where from = 1, to = 50)

// List
Generators.persistent_list_of[U32](Generators.u32())

// Set (structural equality)
Generators.persistent_set_of[U8](Generators.u8() where to = 20)

// SetIs (identity equality)
Generators.persistent_set_is_of[U8](Generators.u8())

// Map (structural equality)
Generators.persistent_map_of[U8, U8](
  Generators.zip2[U8, U8](Generators.u8(), Generators.u8()))

// MapIs (identity equality)
Generators.persistent_map_is_of[U8, U8](
  Generators.zip2[U8, U8](Generators.u8(), Generators.u8()))
```

Element types must satisfy `#share` since persistent collections store `val` references. The set and map generators additionally require keys to be `Hashable` and `Equatable`.

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

## Fix lambda parameter type inference regression

Lambda parameters that relied on type inference from the calling context stopped compiling after the addition of generic type argument inference. Code like `m.upsert("key", 1, {(old, cur) => old + cur })` produced "a lambda parameter must specify a type or be inferable from context" where it previously compiled without error. Lambda parameter types are once again inferred from the expected function type at the call site.

