// Regression test for find_infer_type's TK_ISECTTYPE accumulator
// leak (ponylang/ponyc#5202), symmetric to type-alias-triple-union-
// infer.
//
// `find_infer_type` in `src/libponyc/expr/operator.c` accumulates an
// `i_type` across iterations by calling `type_isect`, which may build
// a freshly-allocated TK_ISECTTYPE root. With three or more variants,
// the third iteration sees an orphaned fresh tree from the second
// iteration as its `prev_i`. `type_isect` rebuilds a new fresh tree
// and discards the previous one — before the fix, that intermediate
// tree was leaked.
//
// Uses structural interfaces so a single `_Impl` class satisfies all
// three components of the intersection.

use @pony_exitcode[None](code: I32)

interface _HasA
  fun a(): U64
interface _HasB
  fun b(): U64
interface _HasC
  fun c(): U64

type _Triple is (_HasA & _HasB & _HasC)

class _Impl
  fun a(): U64 => 1
  fun b(): U64 => 2
  fun c(): U64 => 3

actor Main
  new create(env: Env) =>
    let x: _Triple = _Impl
    let y = x
    if (y.a() + y.b() + y.c()) == 6 then
      @pony_exitcode(1)
    end
