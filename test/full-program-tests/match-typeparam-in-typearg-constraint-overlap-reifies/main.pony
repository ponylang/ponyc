// Regression test for ponylang/ponyc#5860 Shape A.
//
// Two type parameters whose constraints only overlap through a common
// subtype used to be rejected at compile time with "this pattern can
// never match", even when both could reify to the shared member. Here
// the caller reifies both A and B to P2 so the two Cell descriptors
// coincide at runtime and the match fires — exit 42.

use @pony_exitcode[None](code: I32)

primitive P1
primitive P2
primitive P3

class val Cell[A: Any #share]
  let value: A
  new val create(v: A) => value = v

primitive Check
  fun apply[A: (P1 | P2), B: (P2 | P3)](x: Cell[A]): I32 =>
    match x
    | let _: Cell[B] => 42
    else
      1
    end

actor Main
  new create(env: Env) =>
    let c: Cell[P2] = Cell[P2](P2)
    @pony_exitcode(Check[P2, P2](c))
