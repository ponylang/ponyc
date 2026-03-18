## Fix tuple literals not matching correctly in union-of-tuples types

When a tuple literal was assigned to a union-of-tuples type, inner tuple elements that should have been boxed as union values were stored inline instead. This caused `match` to produce incorrect results — the match would fail to recognize valid data, or extract wrong values.

```pony
type DependencyOp is ((Link, ID) | (Unlink, ID) | Spawn)

type Change is (
  (TimeStamp, Principal, NOP, None)
  | (TimeStamp, Principal, Dependency, DependencyOp)
)

// This produced incorrect match results:
let cc: Change = ((0, 0), "bob", Dependency, (Link, "123"))

// Workaround was to construct the inner tuple separately:
let op: DependencyOp = (Link, "123")
let cc: Change = ((0, 0), "bob", Dependency, op)
```

Both forms now produce correct results. The fix also applies when tuple literals are passed directly as function arguments.
