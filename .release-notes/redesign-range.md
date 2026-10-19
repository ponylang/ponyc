## Redesign Range API

`Range` has been redesigned with two named constructors, and `Reverse` has been removed. `Range.till(from, to)` excludes the far bound; `Range.through(from, to)` includes it. Direction is inferred from the arguments, and the step is always a positive magnitude. A zero or negative step, or a non-finite float argument, produces an empty range. Use `Range.through` or `Range.till` with `from > to` for descending ranges.

`is_empty()` and `is_infinite()` have been removed. Infinite ranges are no longer supported.

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
for i in Range.till(0, 10) do ... end

// ascending with step
for i in Range.till(0, 10, 2) do ... end

// descending, inclusive of both endpoints
for e in Range.through(10, 0) do ... end

// descending with step
for e in Range.through(10, 0, 3) do ... end
```
