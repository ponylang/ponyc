// Regression test for ponylang/ponyc#5860 Shape A.
//
// The compile-time loosening for two type parameters whose constraints
// overlap must not accept runtime matches that don't fire. Here A
// reifies to P1 (not in B's constraint) and B reifies to P2, so the
// two Cell descriptors differ and control falls through to the else
// branch — exit 3.

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
    | let _: Cell[B] => 1
    else
      3
    end

actor Main
  new create(env: Env) =>
    let c: Cell[P1] = Cell[P1](P1)
    @pony_exitcode(Check[P1, P2](c))
