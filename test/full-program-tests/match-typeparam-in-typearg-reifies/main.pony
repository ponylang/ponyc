// Regression test for ponylang/ponyc#723.
//
// A pattern whose type argument contains a type parameter used to be
// rejected at compile time with "this pattern can never match", even when
// the parameter could reify to make the pair match at runtime. Here the
// caller reifies both type parameters so the pattern's descriptor equals
// the operand's descriptor and the match fires — exit 42.

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
    | let _: Cell[Wrap[B] val] => 42
    else
      1
    end

actor Main
  new create(env: Env) =>
    let inner = Wrap[U32](7)
    let c: Cell[Wrap[U32] val] = Cell[Wrap[U32] val](inner)
    @pony_exitcode(Check[Wrap[U32] val, U32](c))
