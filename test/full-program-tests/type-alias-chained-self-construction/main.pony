// Regression test for chained type aliases in self-construction reference.
//
// `is_constructed_from` in `src/libponyc/pass/refer.c:266` recognizes the
// `let a: T = a.create(...)` pattern, where `a` appears to be a forward
// reference but is actually a constructor call on the type that `a` is
// declared as. The function unfolds the typeref once and asserts the
// result is TK_NOMINAL with a non-NULL data pointer; for a chained alias
// the one-level unfold returns another TK_TYPEALIASREF, the check fails,
// and `valid_reference` then rejects the program with "can't use an
// undefined variable in an expression".
//
// This is a #5145 regression that the prior per-site fixes did not
// catch. The transitive unfold in `typealias_unfold` produces a concrete
// TK_NOMINAL for chained class aliases, allowing the self-construction
// pattern to type-check correctly. See ponylang/ponyc#5195.

use @pony_exitcode[None](code: I32)

class Foo
  let _v: U64

  new create(v: U64) =>
    _v = v

  fun value(): U64 =>
    _v

type _FooAlias is Foo
type _ChainedFoo is _FooAlias

actor Main
  new create(env: Env) =>
    let a: _ChainedFoo = a.create(42)
    if a.value() == 42 then
      @pony_exitcode(1)
    end
