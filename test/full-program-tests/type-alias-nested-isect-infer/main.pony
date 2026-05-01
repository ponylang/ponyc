// Regression test for find_infer_type's TK_ISECTTYPE accumulator
// leak (ponylang/ponyc#5202), orphaned-`t` path. Symmetric to
// type-alias-nested-union-infer.
//
// The outer intersection contains a typealias variant whose
// TK_TYPEALIASREF case (post-#5203) returns a freshly-dup'd orphan
// tree. The outer TK_ISECTTYPE loop then calls `type_isect(alias_prev,
// orphan_t)`, which builds a new fresh root and discards the orphan —
// before the fix, that orphan was leaked.
//
// Note: `typealias_unfold` is transitive only for chained top-level
// aliases (see #5201). Children of an unfolded intersection retain
// their TK_TYPEALIASREF form, so this nested structure survives into
// the TK_ISECTTYPE iteration as intended.

use @pony_exitcode[None](code: I32)

interface _HasA
  fun a(): U64
interface _HasB
  fun b(): U64
interface _HasC
  fun c(): U64

type _Pair is (_HasB & _HasC)
type _Triple is (_HasA & _Pair)

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
