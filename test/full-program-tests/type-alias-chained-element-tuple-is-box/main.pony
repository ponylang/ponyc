// Regression test for chained type aliases as tuple element types in
// boxed tuple identity comparisons.
//
// `tuple_is_box_element` in `src/libponyc/codegen/genident.c:245-269`
// classifies a tuple element's kind based on `ast_id(check_field)` after
// a one-level unfold. If the element is a chained alias, the unfold
// returns another TK_TYPEALIASREF, the TK_TUPLETYPE check at line 263
// is false, `is_machine_word` is also false, and `field_kind` stays at
// SUBTYPE_KIND_UNBOXED. The generated identity comparison then takes
// the wrong branch (pointer equality instead of numeric equality on the
// element), which is a silent miscompilation rather than a crash.
//
// The `type-alias-chained-tuple-is` test covers `tuple_is` and
// `tuple_is_box` at the outer tuple level, but those sites iterate
// element types that are unfolded by their caller. To reach
// `tuple_is_box_element` with a chained-alias element, the element
// itself must be a chained alias. This test uses a chained alias to
// U64 as one element of a pair, then compares an unboxed value against
// a union-boxed value to force dispatch through
// `tuple_is_box` -> `tuple_is_box_element`.
//
// See ponylang/ponyc#5195.

use @pony_exitcode[None](code: I32)

type _InnerNum is U64
type _ChainedNum is _InnerNum
type _Pair is (_ChainedNum, U64)

actor Main
  new create(env: Env) =>
    let a: _Pair = (U64(1), U64(2))
    let boxed_equal: (_Pair | None) = (U64(1), U64(2))
    let boxed_unequal: (_Pair | None) = (U64(3), U64(4))

    if not (a is boxed_equal) then return end
    if a is boxed_unequal then return end

    @pony_exitcode(1)
