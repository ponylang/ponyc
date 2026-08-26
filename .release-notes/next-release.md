## Fix false "declaration appears after use" error with trait default method bodies

When a type implemented multiple traits that declared the same method, and one of those traits provided a default body, the compiler could reject the program with a false "declaration of 'env' appears after use" error:

```pony
trait Concrete
  fun hello(env: Env) =>
    env.out.print("hello")

trait Abstract1
  fun hello(env: Env)

actor Main is (Abstract1 & Concrete)
  new create(env: Env) =>
    hello(env)
```

Whether the compiler raised the error depended on the order the traits were declared in source. This has been fixed.

## Fix false "this pattern can never match" error for a type parameter inside a type argument

Matching a generic value against a pattern whose type argument contained a type parameter — for example, `Cons[List[B]]` against a value of `Cons[A]` — was rejected at compile time with "this pattern can never match" even when `A` could reify to `List[B]` at runtime:

```pony
class val Cons[A: Any #share]
  let head: A
  let tail: (Cons[A] | Nil[A])
  new val create(h: A, t: (Cons[A] | Nil[A])) =>
    head = h
    tail = t

primitive Nil[A: Any #share]

type List[A: Any #share] is (Cons[A] | Nil[A])

primitive PickHead
  fun apply[B: Any #share](xs: List[List[B]]): (List[B] | None) =>
    match xs
    | let c: Cons[List[B]] => c.head
    else
      None
    end
```

```
Error:
main.pony:14:7: this pattern can never match
    | let c: Cons[List[B]] => c.head
      ^
```

Class, actor, and primitive patterns of this shape are now accepted; the match uses the fully reified type at runtime as it does for any other pattern.

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

A class that structurally satisfies an interface without declaring `is I` is not covered by this fix and still produces the same error — either add `is I[A]` on the class, or wait for a future release.

