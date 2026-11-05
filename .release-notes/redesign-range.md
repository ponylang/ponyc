## Redesign Range API

`Range` has been redesigned with two named constructors, and `Reverse` has been removed. `Range[T].till(from, to)` excludes the far bound; `Range[T].through(from, to)` includes it. Direction is inferred from the arguments, and the step is always a positive magnitude. A zero or negative step, or a non-finite float argument, produces an empty range.

The type parameter no longer defaults to `USize` — every call site must name the numeric type explicitly.

`is_empty()` and `is_infinite()` have been removed. Infinite ranges are no longer supported.

The old `Range(a, b)` produced an empty range when `a > b`. The new `Range[T].till(a, b)` infers a descending range instead. Code that passed dynamic bounds and relied on the empty-when-no-progress behavior needs an explicit guard:

```pony
// Old: Range(start, limit) was empty when start >= limit
// New: Range[USize].till(start, limit) descends when start > limit
if start < limit then
  for i in Range[USize].till(start, limit) do ... end
end
```

Before:

```pony
// ascending, exclusive of 10
for i in Range(0, 10) do ... end

// ascending with step
for i in Range(0, 10, 2) do ... end

// descending, inclusive of both endpoints
for e in Reverse(10, 0) do ... end

// descending with step
for e in Reverse(10, 0, 3) do ... end
```

After:

```pony
// ascending, exclusive of 10
for i in Range[USize].till(0, 10) do ... end

// ascending with step
for i in Range[USize].till(0, 10, 2) do ... end

// descending, inclusive of both endpoints
for e in Range[USize].through(10, 0) do ... end

// descending with step
for e in Range[USize].through(10, 0, 3) do ... end
```
