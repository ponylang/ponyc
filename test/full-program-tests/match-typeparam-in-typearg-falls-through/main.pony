// Regression test for ponylang/ponyc#723.
//
// The compile-time loosening for type parameters inside type arguments must
// not accept matches that don't actually happen at runtime. Here the pattern
// requires the reified type argument to be Wrap[String], but A reifies to
// U8 — the runtime descriptor check must fail this and control must fall
// through to the else branch.

use @pony_exitcode[None](code: I32)

class val Wrap[A: Any #share]
  let value: A
  new val create(v: A) => value = v

class val Cell[A: Any #share]
  let value: A
  new val create(v: A) => value = v

primitive Check
  fun apply[A: Any #share, B: Any #share](x: Cell[A]): I32 =>
    match x
    | let _: Cell[Wrap[B] val] => 1
    else
      2
    end

actor Main
  new create(env: Env) =>
    let c: Cell[U8] = Cell[U8](42)
    @pony_exitcode(Check[U8, String](c))
