// Regression test for find_possible_fun_defs' TK_TYPEALIASREF case (#5200):
// lambda inference through a union-of-interfaces alias with filtering.
//
// The alias unfolds to a union of two single-method interfaces with
// different parameter counts. `find_possible_fun_defs` produces two
// matches (both dup'd from the unfolded tree). The filtering loop in
// `expr_lambda` eliminates `_Fn2` (wrong parameter count); its dup'd
// obj_cap must be freed by `ast_free_unattached` before `continue`.
// This exercises the multi-iteration dup loop and the filtering-loop
// cleanup path.

use @pony_exitcode[None](code: I32)

interface _Fn1
  fun apply(x: U64): U64

interface _Fn2
  fun apply(x: U64, y: U64): U64

type _FnEither is (_Fn1 | _Fn2)

actor Main
  new create(env: Env) =>
    let f: _FnEither = {(x) => x + 1}
    match f
    | let g: _Fn1 => if g(41) == 42 then @pony_exitcode(1) end
    | let _: _Fn2 => None
    end
