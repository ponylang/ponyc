## Fix `Iter.intersperse` looping forever when `n` is 0

`Iter.intersperse(value, 0)` looped forever.

`intersperse` now treats `n == 0` as `n == 1`, matching `step_by`.
