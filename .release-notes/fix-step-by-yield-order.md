## Change Iter.step_by to yield the first element instead of skipping it

`Iter.step_by(n)` now yields the first element of the underlying iterator, then skips `n - 1` elements between each yield. Previously it skipped `n - 1` elements before the first yield.

The old behavior was inconsistent with Pony's own `Range` and with equivalents in Rust (`Iterator::step_by`), Python (`itertools.islice`), and C++20 (`views::stride`).

Before:

```pony
// produced 1, 3, 5, 7, 9
for v in Iter[USize](Range(0, 10)).step_by(2) do
  env.out.print(v.string())
end
```

After:

```pony
// produces 0, 2, 4, 6, 8
for v in Iter[USize](Range(0, 10)).step_by(2) do
  env.out.print(v.string())
end
```

Code that relied on the skip-first behavior can prepend `.skip(n - 1)` before `.step_by(n)` to get the old positions.
