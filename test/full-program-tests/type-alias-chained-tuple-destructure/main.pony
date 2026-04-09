// Regression test for chained type aliases in tuple destructuring
// assignment.
//
// Three sites in `expr_assign` (`src/libponyc/expr/operator.c`),
// `safe_to_move`'s TK_TUPLE case (`src/libponyc/type/safeto.c:115`),
// and `assign_tuple` (`src/libponyc/codegen/genoperator.c:358`) all
// unfold the right-hand-side type once and then dispatch on or iterate
// the unfolded result expecting a concrete TK_TUPLETYPE. For a chained
// alias, the one-level unfold returns another TK_TYPEALIASREF and each
// site fails differently:
//
//   - `expr_assign` at `operator.c:660` hits its `default: pony_assert(0)`
//     in the switch over the unfolded shape (this is the first crash a
//     chained alias hits in the destructure flow).
//   - `safe_to_move` at `safeto.c:122` would assert `TK_TUPLETYPE` after
//     unfolding (covered by this test in the post-fix execution path).
//   - `assign_tuple` at `genoperator.c:410` would assert
//     `rtype_child == NULL` after iterating the wrong children (also
//     covered post-fix).
//
// The pre-fix counterfactual fires first at `operator.c:660`. With the
// fix, all three sites are exercised end-to-end. See ponylang/ponyc#5195.

use @pony_exitcode[None](code: I32)

type _Pair is (U64, U64)
type _Middle is _Pair

actor Main
  new create(env: Env) =>
    let p: _Middle = (U64(1), U64(2))
    (let a, let b) = p
    if (a == 1) and (b == 2) then
      @pony_exitcode(1)
    end
