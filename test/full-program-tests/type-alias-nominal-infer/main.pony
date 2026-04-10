// Regression test for find_infer_type's TK_TYPEALIASREF case (#5199),
// case (a): the recursive find_infer_type call returns the unfolded
// tree itself unchanged.
//
// `find_infer_type` in `src/libponyc/expr/operator.c` dispatches on
// TK_TYPEALIASREF by unfolding the alias and recursing on the result.
// When the right-hand side of an inferring let binding is an aliased
// nominal type with no destructuring path, the recursive call hits the
// default case with `path == NULL` and returns the unfolded pointer
// unchanged. This exercises the `result == unfolded` branch of the fix
// (where the inner free is skipped to avoid a double-free) and confirms
// the dup-and-clear-scope sequence handles the result-equals-unfolded
// case correctly.

use @pony_exitcode[None](code: I32)

type _Count is U64

actor Main
  new create(env: Env) =>
    let x: _Count = U64(42)
    let y = x
    if y == 42 then
      @pony_exitcode(1)
    end
