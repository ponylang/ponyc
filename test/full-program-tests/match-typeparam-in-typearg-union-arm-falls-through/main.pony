// Regression test for ponylang/ponyc#5860 Shape B.
//
// A pattern accepted by the compound-arm loosening must not match at
// runtime when the reification of A doesn't make the two type
// arguments equal. Here A reifies to String, so `Cell[(P | String)]`
// has a different descriptor from `Cell[(P | U8)]` and control falls
// through to the else branch — exit 3.

use @pony_exitcode[None](code: I32)

primitive P

class val Cell[A: Any #share]
  let value: A
  new val create(v: A) => value = v

primitive Check
  fun apply[A: Any #share](x: Cell[(P | A)]): I32 =>
    match x
    | let _: Cell[(P | U8)] => 1
    else
      3
    end

actor Main
  new create(env: Env) =>
    let c: Cell[(P | String)] = Cell[(P | String)]("hi")
    @pony_exitcode(Check[String](c))
