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

