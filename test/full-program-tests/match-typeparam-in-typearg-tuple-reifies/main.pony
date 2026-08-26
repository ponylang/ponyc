// Regression test for ponylang/ponyc#5860.
//
// A type argument that is a tuple containing a type parameter used to
// be rejected at compile time with "this pattern can never match", even
// when the parameter could reify to make the tuple element line up.
// Here A reifies to U8 so `Cell[(P, A)]` and `Cell[(P, U8)]` share a
// descriptor at runtime and the match fires — exit 42.

use @pony_exitcode[None](code: I32)

primitive P

class val Cell[A: Any #share]
  let value: A
  new val create(v: A) => value = v

primitive Check
  fun apply[A: Any #share](x: Cell[(P, A)]): I32 =>
    match x
    | let _: Cell[(P, U8)] => 42
    else
      1
    end

actor Main
  new create(env: Env) =>
    let c: Cell[(P, U8)] = Cell[(P, U8)]((P, 7))
    @pony_exitcode(Check[U8](c))
