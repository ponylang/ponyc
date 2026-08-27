## Fix false unreachable match for structural interface patterns with lambda parameters

Matching a class against a structural interface pattern produced a false "this pattern can never match" error when the class and the interface used structurally equivalent but nominally different single-method interface types in their method parameters. The most common case is a method with a lambda parameter type (`{(A): B} val`) matched against a pattern whose interface uses either a lambda written at a different source position or a user-defined interface with the same method signature:

```pony
class val Wrap[A: Any #share]
  let _value: A
  new val create(v: A) => _value = v
  fun value(): A => _value

interface val Mapper[A: Any #share]
  fun map_it[B: Any #share](f: {(A): B} val): B

class val Box[A: Any #share]
  let _v: A
  new val create(v: A) => _v = v
  fun map_it[B: Any #share](f: {(A): B} val): B => f(_v)

primitive Check
  fun apply[A: Any #share, B: Any #share](x: Box[A]): I32 =>
    match x
    | let _: Mapper[Wrap[B] val] val => 42
    else
      1
    end
```

```
Error:
main.pony:17:7: this pattern can never match
    | let _: Mapper[Wrap[B] val] val => 42
      ^
```

Single-method interfaces with different definitions but identical method signatures are now compared structurally rather than by identity.
