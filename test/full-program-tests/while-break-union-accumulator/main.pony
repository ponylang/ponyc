// Regression test for expr_while accumulator leak (ponylang/ponyc#5210).
//
// expr_while reads the loop's type (which may be non-NULL from prior break
// expressions) and accumulates body and else-clause types via
// control_type_add_branch. When the break type, body type, and else type
// are all distinct, the first accumulation builds a fresh union tree, and
// the second accumulation builds another — orphaning the intermediate
// tree before the fix.

use @pony_exitcode[None](code: I32)

primitive _Go
  fun apply(): Bool => true

actor Main
  new create(env: Env) =>
    let x: (U8 | U16 | U32) =
      while true do
        if _Go() then break U8(1) end
        U16(2)
      else
        U32(3)
      end
    match x
    | let _: U8 => @pony_exitcode(1)
    | let _: U16 => @pony_exitcode(1)
    | let _: U32 => @pony_exitcode(1)
    end
