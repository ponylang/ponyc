## Fix false "this pattern can never match" error for constraint-overlapping type parameters and union, intersection, or tuple type arguments

Three more shapes where a match with generic type arguments used to compile-error with "this pattern can never match" now compile and match at runtime when the reified types coincide.

The first is a pair of type parameters whose constraints don't relate by subtyping but share a common inhabitant. Neither `(P1 | P2)` nor `(P2 | P3)` is a subtype of the other, but both admit `P2`, so a reification `A = B = P2` makes the two `Cell` types equal:

```pony
primitive P1
primitive P2
primitive P3

class val Cell[A: Any #share]
  let value: A
  new val create(v: A) => value = v

primitive Check
  fun apply[A: (P1 | P2), B: (P2 | P3)](x: Cell[A]) =>
    match x
    | let _: Cell[B] => None
    else
      None
    end
```

The second is a type argument that is a union or intersection literal containing a type parameter. Here `A` reifies to `U8` so the operand and pattern share a runtime descriptor:

```pony
primitive P

class val Cell[A: Any #share]
  let value: A
  new val create(v: A) => value = v

primitive Check
  fun apply[A: Any #share](x: Cell[(P | A)]) =>
    match x
    | let _: Cell[(P | U8)] => None
    else
      None
    end
```

The third is a tuple type argument containing a type parameter, handled the same way — element by element:

```pony
primitive Check2
  fun apply[A: Any #share](x: Cell[(P, A)]) =>
    match x
    | let _: Cell[(P, U8)] => None
    else
      None
    end
```

Matches where no reification could ever satisfy the type-argument pair still reject at compile time.
