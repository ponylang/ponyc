## Fix Iter.cycle() reporting has_next() = true on an empty source iterator

`Iter.cycle()` on an empty iterator reported `has_next() = true` indefinitely while every call to `next()` raised an error. Code following the `has_next()` protocol — including `for` loops and other `Iter` adapters like `chain` — either looped forever or, in the case of `chain`, silently dropped every iterator after the empty cycle in the pipeline.

`cycle()` now returns `false` from `has_next()` when the source is empty.
