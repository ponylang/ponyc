// Regression test for ponylang/ponyc#5863.
//
// The compile-time loosening for a structural interface pattern whose type
// argument contains a type parameter must not accept matches that don't
// actually happen at runtime. Here the pattern requires the reified type
// argument to be Wrap[String], but A reifies to U8 — Cons[U8]'s method
// `fun value(): U8` does not structurally satisfy I[Wrap[String] val]'s
// `fun value(): Wrap[String] val`, so the match must fall through to the
// else branch — exit 2.

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
    | let _: I[Wrap[B] val] val => 1
    else
      2
    end

actor Main
  new create(env: Env) =>
    let c: Cons[U8] = Cons[U8](42)
    @pony_exitcode(Check[U8, String](c))
