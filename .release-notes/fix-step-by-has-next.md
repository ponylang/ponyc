## Fix Iter.step_by producing errors on short remainders

`Iter.step_by(n)` could error when fewer than `n` elements remained in the source iterator. `has_next()` returned `true` because the source still had elements, but `next()` failed trying to skip past them. This broke downstream methods — `last()` errored, `.interleave()` silently lost elements from the other iterator, and `count()` undercounted.

```pony
// was: errored (has_next true, next errors on the trailing element)
// now: returns 1
Iter[USize]([as USize: 0; 1; 2].values()).step_by(2).last()?
```
