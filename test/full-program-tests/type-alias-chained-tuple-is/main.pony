// Regression test for chained type aliases in identity comparisons.
//
// Three call sites in genident.c unfolded aliases only one level:
//
// - `tuple_is` (genident.c): asserted `ast_id(left_type) == TK_TUPLETYPE`
//   after unfolding. A chained alias left both operands as
//   TK_TYPEALIASREF, tripping the assertion directly.
//
// - `tuple_is_box` (genident.c): computed `ast_childcount(left_type)` as
//   the tuple cardinality. For a chained alias this returned the alias
//   ref's child count (TK_ID + typeargs + cap + eph = 4), miscomparing
//   against descriptor field counts and then iterating alias children as
//   tuple elements.
//
// - `tuple_is_box_element` (genident.c): classified `field_kind` via
//   `ast_id(check_field) == TK_TUPLETYPE`. A chained alias fell through
//   to SUBTYPE_KIND_UNBOXED, generating the wrong dispatch for later box
//   comparisons.
//
// See ponylang/ponyc#5195. The fix made typealias_unfold transitive.
//
// This test asserts correct behavior on both the tuple_is path (two
// unboxed chained-alias tuples) and the tuple_is_box path (unboxed
// chained-alias tuple compared to a boxed chained-alias tuple inside a
// union). Running under debug asserts the crashes don't fire; the exit
// code asserts the comparison results are correct, catching any
// miscompilation where a wrong tuple `cardinality` would still compile
// but produce the wrong answer. The `tuple_is_box_element` call site
// is not directly exercised here — it is reached only when a tuple
// element is itself a chained alias — but the transitive helper fix
// closes that site by construction.

use @pony_exitcode[None](code: I32)

type _Pair is (U64, U64)
type _Middle is _Pair

actor Main
  new create(env: Env) =>
    // tuple_is path: two unboxed _Middle tuples, equal and unequal.
    let a: _Middle = (U64(1), U64(2))
    let b: _Middle = (U64(1), U64(2))
    let c: _Middle = (U64(3), U64(4))

    if not (a is b) then return end
    if a is c then return end

    // tuple_is_box path: unboxed _Middle compared with a boxed tuple
    // carried in a union. `a is boxed_equal` should be true;
    // `a is boxed_unequal` should be false.
    let boxed_equal: (_Middle | None) = (U64(1), U64(2))
    let boxed_unequal: (_Middle | None) = (U64(3), U64(4))

    if not (a is boxed_equal) then return end
    if a is boxed_unequal then return end

    @pony_exitcode(1)
