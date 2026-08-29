## Add multi-iterator for loop sugar

Pony's `for` loop now accepts multiple iterators, zipping them together:

```pony
for (a, b) in (iter_a, iter_b) do
  env.out.print(a.string() + " " + b.string())
end
```

The loop runs until the shortest iterator is exhausted. Three binding forms are supported: destructured names matching the iterator count, nested destructuring for iterators that yield tuples, and a single name that receives the full tuple of values.
