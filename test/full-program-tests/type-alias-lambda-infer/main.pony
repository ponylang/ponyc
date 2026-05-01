// Regression test for find_possible_fun_defs' TK_TYPEALIASREF case (#5200):
// lambda parameter inference through a type alias.
//
// `find_possible_fun_defs` in `src/libponyc/expr/lambda.c` dispatches on
// TK_TYPEALIASREF by unfolding the alias and recursing on the result.
// When the unfolded result is a TK_NOMINAL pointing to a single-method
// interface, the recursion stores a pointer to child 3 (the obj_cap) of
// that nominal — which lives inside the unfolded tree. The fix dups the
// obj_cap and frees the unfolded tree, closing the leak.
//
// This test exercises the path by using a type alias as the antecedent
// type for a lambda with inferred parameter types. The lambda's parameter
// type and obj_cap are inferred from the alias's unfolded interface.

use @pony_exitcode[None](code: I32)

interface _Fn
  fun apply(x: U64): U64

type _FnAlias is _Fn

actor Main
  new create(env: Env) =>
    let f: _FnAlias = {(x) => x + 1}
    if f(41) == 42 then
      @pony_exitcode(1)
    end
