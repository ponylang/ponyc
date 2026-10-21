## Fix Iter.dedup() and Iter.unique() returning has_next() = true when exhausted

`Iter.dedup()` and `Iter.unique()` reported `has_next() = true` when the remaining source elements were all duplicates. `next()` would then error, violating the `Iterator` contract. This caused `last()`, `nth()`, and `find()` to error on valid input when trailing elements were duplicates.

```pony
// was: error
// now: 0
Iter[U64]([as U64: 0; 0].values()).dedup[HashEq[U64]]().last()?

// was: error
// now: 2
Iter[U64]([as U64: 1; 2; 1].values()).unique[HashEq[U64]]().last()?
```
