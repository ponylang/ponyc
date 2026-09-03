## Fix filter generator stack overflow from unbounded recursion

`Generator.filter` could crash or hang when the predicate rejected many consecutive values.

`filter` now retries up to a configurable limit (default 1000) and raises an error when exhausted. A new `max_attempts` parameter controls the limit:

```pony
Generators.u32().filter(
  {(u) => (u, u > 250) }
  where max_attempts = 5000)
```

The default is high enough that a predicate matching 1% of values succeeds with near-certainty.
