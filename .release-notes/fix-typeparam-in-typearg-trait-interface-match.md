## Fix false "this pattern can never match" error for a type parameter inside a trait or interface type argument

Matching a class value against a trait or interface pattern whose type argument contained a type parameter — for example, `T[Wrap[B] val]` against a value of `Cons[A]` where `Cons` provides `T` — was rejected at compile time with "this pattern can never match" even when `A` could reify to `Wrap[B] val` at runtime:

```pony
class val Wrap[A: Any #share]
  let value: A
  new val create(v: A) => value = v

trait val T[A: Any #share]
  fun get_value(): A

class val Cons[A: Any #share] is T[A]
  let _v: A
  new val create(v: A) => _v = v
  fun get_value(): A => _v

primitive Check
  fun apply[A: Any #share, B: Any #share](x: Cons[A]) =>
    match x
    | let _: T[Wrap[B] val] val => None
    else
      None
    end
```

```
Error:
main.pony:16:7: this pattern can never match
    | let _: T[Wrap[B] val] val => None
      ^
```

Trait and interface patterns of this shape are now accepted when the operand's class nominally provides the pattern's trait or interface via `is`; the match uses the fully reified type at runtime as it does for any other pattern.

A class that structurally satisfies an interface without declaring `is I` is not covered by this fix and still produces the same error — either add `is I[A]` on the class, or wait for a future release. The other shapes named in the earlier entity-side fix — union or intersection types appearing in a type argument, and two type parameters whose constraints only overlap through a common subtype — are also still tracked as follow-up work.
