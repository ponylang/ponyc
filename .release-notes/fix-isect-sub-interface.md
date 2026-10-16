## Fix subtype checking for intersections that collectively satisfy an interface

The compiler rejected an intersection type `(A & B)` as a subtype of an interface when neither `A` nor `B` alone provides every method the interface requires, even when together they cover all of them.

```pony
interface Sized
  fun rows(): USize
  fun cols(): USize

class HasRows
  fun rows(): USize => 10

class HasCols
  fun cols(): USize => 20

primitive Checker
  fun check(s: Sized) => None

  fun example(x: (HasRows & HasCols)) =>
    check(x) // previously rejected: neither HasRows nor HasCols alone is Sized
```

The call `check(x)` failed with a subtype error because the compiler checked each intersection member individually. It now checks the intersection's members collectively against the interface's method list, so `(HasRows & HasCols)` is accepted as a subtype of `Sized`.
