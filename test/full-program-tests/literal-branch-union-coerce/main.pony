// Regression test for coerce_control_block's block_type accumulator
// leak (ponylang/ponyc#5204).
//
// `coerce_control_block` in `src/libponyc/expr/literal.c` accumulates a
// `block_type` across iterations by calling `type_union` for each
// TK_LITERALBRANCH child. With three branches, the third iteration sees
// an orphaned fresh tree (from the second iteration's union of the first
// two branch types) as its `prev_block`. `type_union` builds a new
// fresh tree and discards the previous one — before the fix, that
// intermediate tree was leaked.
//
// An `if`/`elseif`/`else` chain with three or more branches whose
// literal types are distinct (none is a subtype of another) exercises
// the multi-iteration accumulator path.

use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let x: (U8 | U16 | U32) =
      if true then
        U8(1)
      elseif false then
        U16(2)
      else
        U32(3)
      end
    match x
    | let _: U8 => @pony_exitcode(1)
    | let _: U16 => None
    | let _: U32 => None
    end
