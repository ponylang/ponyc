## Fix Iter.take_while() returning rejected elements after has_next() returns false

`Iter.take_while()` had a bug where `has_next()` returned false on the first call after the predicate rejected an element, but true on the second call, and `next()` then yielded the rejected element. This broke `nth()` and any code that called `has_next()` more than once after the predicate failed.

```pony
// was: has_next() false, then true; next() returned 5
// now: has_next() stays false
let tw = Iter[U64]([as U64: 5; 1].values())
  .take_while({(x) => x < 3 })
tw.has_next() // false
tw.has_next() // false (was true)

// was: returned 5 (a rejected element)
// now: error
Iter[U64]([as U64: 5; 1].values())
  .take_while({(x) => x < 3 })
  .nth(1)?
```
