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

Class, actor, and primitive patterns of this shape are now accepted; the match uses the fully reified type at runtime as it does for any other pattern. Trait and interface patterns still produce the same false error, as do a few less common shapes — union or intersection types appearing in a type argument, and two type parameters whose constraints only overlap through a common subtype. All are tracked as follow-up work.
