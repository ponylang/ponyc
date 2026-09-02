## Fix stack overflow in itertools Iter methods

Several methods on `Iter` in the `itertools` package used unbounded recursion internally and could crash on larger iterators. This has been fixed.
