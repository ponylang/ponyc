## Reject tuple types hidden in an intersection within a type constraint

Tuple types can't be used as generic type constraints. The compiler already rejected a tuple smuggled into a constraint through a type alias, including when it was hidden inside a union. It did not, however, catch a tuple hidden inside an intersection, so the following incorrectly compiled:

```pony
type R is (U8 & (U8, U32))
class Block[T: R]
```

The compiler now rejects this with the same error it already gives for tuples in unions:

```
constraint contains a tuple; tuple types can't be used as type constraints
```
