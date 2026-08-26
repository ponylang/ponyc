## Fix false unreachable match for structural interface patterns

The fix for trait and interface match patterns with a type parameter inside a type argument only handled classes that nominally provide the interface via `is I[A]`. A class that structurally satisfies the interface — same methods, same signatures, no `is` declaration — still produced the false error:

```pony
class val Wrap[A: Any #share]
  let _value: A
  new val create(v: A) => _value = v
  fun value(): A => _value

interface val I[A: Any #share]
  fun value(): A

class val Cons[A: Any #share]
  let _v: A
  new val create(v: A) => _v = v
  fun value(): A => _v

primitive Check
  fun apply[A: Any #share, B: Any #share](x: Cons[A]): I32 =>
    match x
    | let _: I[Wrap[B] val] val => 1
    else
      2
    end
```

```
Error:
main.pony:17:7: this pattern can never match
    | let _: I[Wrap[B] val] val => 1
      ^
```

Structural interface patterns with type parameters in type arguments are now accepted. As with the nominal case, the match uses the fully reified type at runtime.
