// Regression test for find_possible_fun_defs' TK_TYPEALIASREF case (#5200):
// lambda with explicit obj_cap through a type alias.
//
// When the lambda specifies its own obj_cap (the trailing `ref`), the
// inference in `expr_lambda` skips the `ast_replace` for obj_cap because
// it's already set. The dup'd obj_cap from the alias unfold remains an
// orphan and must be freed by `ast_free_unattached` in the cleanup
// iteration. This exercises the "actually frees" path of that cleanup.

use @pony_exitcode[None](code: I32)

interface _Fn
  fun apply(x: U64): U64

type _FnAlias is _Fn

actor Main
  new create(env: Env) =>
    let f: _FnAlias = {(x) => x + 1} ref
    if f(41) == 42 then
      @pony_exitcode(1)
    end
