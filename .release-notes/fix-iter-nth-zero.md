## Fix Iter.nth and Iter.find to error when n is 0

`Iter.nth` and `Iter.find` count positions from 1. Calling `nth(0)` returned the first element instead of raising an error, making it behave identically to `nth(1)`. Calling `find(predicate, 0)` wrapped its internal counter to `USize.max_value()`, exhausted the iterator, and then raised.

Both methods now error immediately when `n` is 0.
