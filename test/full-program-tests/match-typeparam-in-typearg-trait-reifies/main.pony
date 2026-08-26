// Regression test for ponylang/ponyc#5859.
//
// A trait pattern whose type argument contains a type parameter used to
// be rejected at compile time with "this pattern can never match", even
// when the operand's class nominally provides the trait and the type
// parameter could reify to make the pair match at runtime. Here the
// caller reifies both type parameters so the reachable-trait bit for
// T[Wrap[U32] val] is set on Cons[Wrap[U32] val]'s descriptor and the
// match fires — exit 42.

use @pony_exitcode[None](code: I32)

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
  fun apply[A: Any #share, B: Any #share](x: Cons[A]): I32 =>
    match x
    | let _: T[Wrap[B] val] val => 42
    else
      1
    end

actor Main
  new create(env: Env) =>
    let inner = Wrap[U32](7)
    let c: Cons[Wrap[U32] val] = Cons[Wrap[U32] val](inner)
    @pony_exitcode(Check[Wrap[U32] val, U32](c))
