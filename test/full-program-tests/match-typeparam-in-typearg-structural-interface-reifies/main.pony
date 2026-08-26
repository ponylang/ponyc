// Regression test for ponylang/ponyc#5863.
//
// A structural interface pattern whose type argument contains a type
// parameter used to be rejected at compile time with "this pattern can
// never match", even when the class satisfies the interface structurally
// (no `is I` declaration) and the type parameter could reify to make the
// match fire at runtime. Here the caller reifies both type parameters so
// Cons[Wrap[U32] val]'s method `fun value(): Wrap[U32] val` structurally
// satisfies I[Wrap[U32] val]'s `fun value(): Wrap[U32] val`, the
// interface bitmap bit is set, and the match fires — exit 42.

use @pony_exitcode[None](code: I32)

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
    | let _: I[Wrap[B] val] val => 42
    else
      1
    end

actor Main
  new create(env: Env) =>
    let inner = Wrap[U32](7)
    let c: Cons[Wrap[U32] val] = Cons[Wrap[U32] val](inner)
    @pony_exitcode(Check[Wrap[U32] val, U32](c))
