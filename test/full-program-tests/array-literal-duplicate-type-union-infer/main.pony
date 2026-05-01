// Regression test for expr_array's element type accumulator cleanup guards
// (ponylang/ponyc#5205).
//
// Exercises the subtype-collapse path in type_union: when consecutive
// elements have the same type, type_union returns one of its inputs
// unchanged rather than building a fresh tree. The identity guards
// (type != prev_type, type != c_type) must correctly skip freeing in
// this case. ast_free_unattached is a no-op on parented nodes, so this
// is defense-in-depth — but an incorrect guard could cause a double-free
// if the invariant were ever weakened.
//
// Companion to array-literal-triple-union-infer, which exercises the
// orphaned-intermediate-tree path with three distinct types.

use @pony_exitcode[None](code: I32)

primitive _A
primitive _B

actor Main
  new create(env: Env) =>
    let arr = [_A; _A; _B]
    try
      match arr(0)?
      | _A => @pony_exitcode(1)
      | _B => None
      end
    end
