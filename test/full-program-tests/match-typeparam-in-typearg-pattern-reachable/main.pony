// Regression test for ponylang/ponyc#723.
//
// A pattern accepted by the loosening at compile time must be reachable at
// codegen — if the reified pattern type is not otherwise instantiated in
// the program, matching still has to compile. Cell[Wrap[U32] val] appears
// only in the pattern here; the program compiles and runs. The match
// itself falls through (A reifies to U8, not Wrap[U32] val) — exit 3.

use @pony_exitcode[None](code: I32)

class val Wrap[A: Any #share]
  let value: A
  new val create(v: A) => value = v

class val Cell[A: Any #share]
  let value: A
  new val create(v: A) => value = v

primitive Check
  fun apply[A: Any #share](x: Cell[A]): I32 =>
    match x
    | let _: Cell[Wrap[U32] val] => 1
    else
      3
    end

actor Main
  new create(env: Env) =>
    let c: Cell[U8] = Cell[U8](7)
    @pony_exitcode(Check[U8](c))
