// Regression test for expr_cases accumulator leak (ponylang/ponyc#5210).
//
// expr_cases in match.c accumulates a type across iterations by calling
// control_type_add_branch for each case body. With three cases whose body
// types are distinct (no subtype relationship), the second iteration's
// type_union builds a fresh tree, and the third iteration's type_union
// builds another — orphaning the intermediate tree from the second
// iteration before the fix.

use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let x: (U8 | U16 | U32) = U8(1)
    match x
    | let a: U8 =>
      @pony_exitcode(1)
    | let b: U16 =>
      @pony_exitcode(2)
    | let c: U32 =>
      @pony_exitcode(3)
    end
