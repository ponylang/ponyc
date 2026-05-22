#include <gtest/gtest.h>
#include <platform.h>

#include <chrono>
#include <string>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "ir"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))


class RecursiveTypeAliasTest : public PassTest
{};


TEST_F(RecursiveTypeAliasTest, MutualWithUnionBaseCase)
{
  // Mutual recursion through type parameters with a union base case in
  // the second alias.
  const char* src =
    "type B is Array[C]\n"
    "type C is (Array[B] | None)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, JsonLikePatternWithoutCollections)
{
  // The JSON-pattern shape using only Array (in builtin). Map needs
  // collections which the test fixture doesn't fully load.
  const char* src =
    "type JsonValue is (String | F64 | Bool | None | JsonArray)\n"
    "type JsonArray is Array[JsonValue]\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, RecursiveAliasUsedAsParameterType)
{
  // A recursive alias as a method parameter type forces the type
  // through subtype, alias, viewpoint, and matchtype paths during
  // expression typechecking. Exercises the cycle-protection wrappers
  // beyond the legality pass.
  const char* src =
    "type Tree is (None | Array[Tree])\n"

    "class Walker\n"
    "  fun walk(t: Tree): None =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let t: Tree = None\n"
    "    Walker.walk(t)";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, RecursiveAliasInClassField)
{
  // A recursive alias used as a class field type. Drives reach to
  // build a descriptor for Holder containing a Tree-typed field, and
  // drives codegen for the field's read into a local. The receiver
  // here is a concrete class (Holder) — for the alias-typed-receiver
  // path through gen_fieldptr, see FieldAccessOnAliasTypedReceiver.
  const char* src =
    "type Tree is (None | Array[Tree])\n"

    "class Holder\n"
    "  var t: Tree\n"
    "  new create() =>\n"
    "    t = None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let h: Holder = Holder\n"
    "    let value: Tree = h.t";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, ChainedAliasToTuple)
{
  // type Pair is OtherPair; type OtherPair is (U64, U64). A two-level
  // alias chain resolving to a tuple. Several callers used to do a
  // single-level unfold and check ast_id == TK_NOMINAL or treat the
  // result as a tuple — chained aliases would silently miscompile.
  // Regression coverage for the unfold-chain fix.
  const char* src =
    "type Pair is OtherPair\n"
    "type OtherPair is (U64, U64)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let p: Pair = (1, 2)\n"
    "    (let a, let b) = p\n"
    "    let other: Pair = (a, b)";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, ChainedAliasToClassConstructorCall)
{
  // type Foo is Bar; type Bar is C. Calling Foo.create() must resolve
  // through the chain to C.
  const char* src =
    "class C\n"
    "  new create() =>\n"
    "    None\n"

    "type Foo is Bar\n"
    "type Bar is C\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let c: Foo = Foo";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, AliasUsedAsTypeparamConstraintCompiles)
{
  // Smoke test for D4. The legality pass enforces D4 passively —
  // alias_body() returns just the provides body (excluding the
  // alias's own typeparams), so the graph walk never reaches a
  // typeparam constraint position. Constructing a program where the
  // exclusion is observable would require the walk to traverse
  // typeparams, which it doesn't, so this test cannot meaningfully
  // counterfactual D4. It does at least confirm that an alias used
  // as a typeparam constraint compiles end-to-end.
  const char* src =
    "type ConstraintHelper is (String | None)\n"

    "class Box[A: ConstraintHelper]\n"
    "  new create() =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, AliasReferencedFromArrowMethodReturnCompiles)
{
  // Smoke test adjacent to D5. Arrow LHS is restricted to caps,
  // 'this', and typeparam refs by Pony's syntax — putting an alias
  // there isn't expressible. So the LHS exclusion isn't directly
  // testable. The arrow RHS classification (a non-constructive
  // recursion position when the RHS is the recursive alias itself)
  // is exercised by TypeAliasArrowRhsRecursion in badpony.cc. This
  // is just a smoke test that aliases interact with arrow types in
  // method signatures.
  const char* src =
    "type ArrowAlias is (None | String)\n"

    "class C\n"
    "  fun ref viewof(): ArrowAlias =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, ThreeAliasMutualRecursion)
{
  // Three-alias mutual recursion (size-3 SCC). Exercises Tarjan
  // bookkeeping that singletons and pairs don't.
  const char* src =
    "type A is (Array[B] | None)\n"
    "type B is (Array[C] | None)\n"
    "type C is (Array[A] | None)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, NonRecursiveAliasNotFalseFlagged)
{
  // Smoke: a trivial non-recursive alias must compile (legality pass
  // must not false-positive).
  const char* src =
    "type Plain is U64\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let n: Plain = 42";

  TEST_COMPILE(src);
}


// The next three tests pin the suggested fixes from the legality-pass
// error messages. The illegal forms live in badpony.cc; if the rule
// changes such that the suggested fix no longer compiles, the error
// text in pass/typealias_recursion.c::report_illegal_cycle has gone
// stale and these tests will catch it.

TEST_F(RecursiveTypeAliasTest, FixForDirectUnionSelfReference)
{
  // Illegal form: type A is (A | None).
  // Error suggests threading the recursion through a class's
  // type argument, e.g., 'Array[A]'.
  const char* src =
    "type A is (Array[A] | None)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}

TEST_F(RecursiveTypeAliasTest, FixForRecursionThroughTuple)
{
  // Illegal form: type IntList is (None | (U32, IntList)).
  // Error suggests the same fix: thread through a class's type
  // argument. Replacing the tuple with Array[IntList] gives a finite
  // layout (different data structure than a list, but legal).
  const char* src =
    "type IntList is (None | Array[IntList])\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}

TEST_F(RecursiveTypeAliasTest, FixForRecursionWithoutBaseCase)
{
  // Illegal form: type A is Array[A].
  // Error suggests adding a non-recursive alternative in a union,
  // e.g., 'type A is (None | <body>)'.
  const char* src =
    "type A is (None | Array[A])\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}

// The next three tests cover programs where the recursive alias has
// a base case that lives across an alias-reference boundary (in the
// body of a non-recursive wrap alias rather than in a union node
// directly inside the recursive alias's body).
// subtree_has_base_case_union in pass/typealias_recursion.c follows
// cross-SCC alias references with a visited set so these compile.

TEST_F(RecursiveTypeAliasTest, BaseCaseAcrossWrapAlias)
{
  // Wrap[T] = (None | Array[T]); X = Wrap[X]. X morally unfolds to
  // (None | Array[X]) — has None as a base case. The recursive
  // alias's body is a typealiasref, not a union node, so the walker
  // has to follow Wrap to find the union.
  const char* src =
    "type Wrap[T] is (None | Array[T])\n"
    "type X is Wrap[X]\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}

TEST_F(RecursiveTypeAliasTest, BaseCaseInUnionMembersThroughWrap)
{
  // X = (WrapA[X] | WrapB[X]). Top-level union exists in X's body,
  // but its members are typealiasrefs whose alternative arms provide
  // the actual base cases. The walker has to follow WrapA and WrapB
  // to recognize that each contributes a non-recursive arm.
  const char* src =
    "type WrapA[T] is (None | Array[T])\n"
    "type WrapB[T] is (String | Array[T])\n"
    "type X is (WrapA[X] | WrapB[X])\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}

TEST_F(RecursiveTypeAliasTest, MutualSccThroughWrapWithBaseCase)
{
  // type Wrap[T] is (None | Array[T]); type A is Wrap[B]; type B is
  // Wrap[A]. SCC = {A, B}, with both edges going through Wrap's
  // typearg position. Wrap's body has None as a base case. The
  // base-case walker has to follow Wrap (cross-SCC) from EITHER A
  // or B and find the same union. Tests the visited set's handling
  // when the same wrap alias is reached from multiple SCC members.
  const char* src =
    "type Wrap[T] is (None | Array[T])\n"
    "type A is Wrap[B]\n"
    "type B is Wrap[A]\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, PhantomTypeargInWrapAlias)
{
  // type Wrap[T] is (None | Array[U64]); type X is Wrap[X].
  // Wrap discards its typeparam — T is never mentioned in the body.
  // The unfolded form is (None | Array[U64]), no recursion. The
  // legality pass should not classify X as recursive at all because
  // the typearg slot doesn't propagate X.
  const char* src =
    "type Wrap[T] is (None | Array[U64])\n"
    "type X is Wrap[X]\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, NestedWrapAliasesProvideBaseCase)
{
  // Two alias-ref hops to the base case. X = Outer[X] = Inner[X] =
  // (None | Array[X]). Tests that the walker can chain through
  // multiple non-recursive alias bodies.
  const char* src =
    "type Inner[T] is (None | Array[T])\n"
    "type Outer[T] is Inner[T]\n"
    "type X is Outer[X]\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, RecursiveAliasUnderArrowViewpoint)
{
  // type A is (None | Array[box->A]). A appears as the RHS of an
  // arrow viewpoint inside Array's typearg. The legality pass accepts
  // this (A -> A constructive via Array, base case None in the
  // union). The cycle protection in viewpoint_type was hitting an
  // is_assumption_match that didn't recognize cap-only leaves
  // (TK_BOX, TK_ISO, TK_REF, etc. used as arrow LHS), so two
  // identical 'box -> A' assumptions never matched and the cycle
  // recursed until stack exhaustion. Regression test: confirms the
  // arrow LHS leaf nodes match correctly.
  const char* src =
    "type A is (None | Array[box->A])\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, RecursiveAliasInMatchExpression)
{
  // type A is (None | Array[A] val). Pattern matching on a value of A
  // forces matchtype to compare the recursive union operand against
  // each pattern. This exercises the cycle-protected matchtype path
  // and (verified empirically by instrumenting is_assumption_match)
  // routes through the TK_UNIONTYPE compound-key branch many times
  // per match. Coverage test: confirms matchtype handles recursive
  // unions without falling into infinite recursion or rejecting
  // correctly-typed pattern arms.
  const char* src =
    "type A is (None | Array[A] val)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: A = None\n"
    "    match x\n"
    "    | None => None\n"
    "    | let arr: Array[A] val => None\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, RecursiveAliasUnderArrowViewpointAllCaps)
{
  // Same shape as RecursiveAliasUnderArrowViewpoint, broadened across
  // every cap leaf is_assumption_match has to match: iso, trn, ref,
  // val, tag, box. Each leaf appears once as an arrow LHS in a
  // recursive alias. If is_assumption_match drops a cap kind, the
  // co-inductive cycle in viewpoint_type fails to find a match and
  // recurses until stack exhaustion.
  const char* src =
    "type A1 is (None | Array[iso->A1])\n"
    "type A2 is (None | Array[trn->A2])\n"
    "type A3 is (None | Array[ref->A3])\n"
    "type A4 is (None | Array[val->A4])\n"
    "type A5 is (None | Array[tag->A5])\n"
    "type A6 is (None | Array[box->A6])\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, FieldAccessOnAliasTypedReceiver)
{
  // codegen/genreference.c::make_fieldptr asserts the receiver type
  // is TK_NOMINAL. deferred_reify reifies typeparams but doesn't
  // unfold typealiasrefs, so an alias-typed receiver (e.g.,
  // `let x: AliasInner = ...; x.value`) carried the TK_TYPEALIASREF
  // through to make_fieldptr and crashed. Regression test for the
  // assertion fire.
  const char* src =
    "class Inner\n"
    "  let value: U32 = 42\n"

    "type AliasInner is Inner\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: AliasInner = Inner\n"
    "    let v = x.value";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, FieldAssignmentOnAliasTypedReceiver)
{
  // type/safeto.c::safe_to_move and safe_to_mutate both early-return
  // false when ast_id(l_type) != TK_NOMINAL for FVARREF/FLETREF/EMBEDREF.
  // For an alias-typed receiver, l_type is TK_TYPEALIASREF, and the
  // assignment was rejected as "left side is immutable" even when the
  // alias resolves to a class. Both call sites now unfold first.
  const char* src =
    "class Foo\n"
    "  var x: U32 = 0\n"

    "type AliasFoo is Foo\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let f: AliasFoo = Foo\n"
    "    f.x = 42";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, EmbedFieldWithChainedAliasType)
{
  // verify/type.c::embed_struct_field_has_finaliser asserts the
  // field type is TK_NOMINAL, but for an alias-typed embed field the
  // ast_type carries the TK_TYPEALIASREF through to the verify pass.
  // Two-link chain (AliasA -> AliasB -> InnerStruct) so the unfold
  // path must follow more than one hop, exercising both the verify
  // assertion fix and the transitive typealias_unfold contract.
  const char* src =
    "struct InnerStruct\n"
    "  var x: U32 = 0\n"

    "type AliasA is AliasB\n"
    "type AliasB is InnerStruct\n"

    "struct Outer\n"
    "  embed inner: AliasA\n"
    "  new create() =>\n"
    "    inner = InnerStruct\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let o = Outer";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, JsonLikePatternWithNamedLeafAliases)
{
  // The canonical JSON-shape recursive alias, with each leaf type
  // given its own name. JsonEntity, JsonArray, and the leaves form a
  // 2-node SCC (JsonEntity, JsonArray) with non-recursive arms in the
  // entity union providing the base case. The named leaf aliases each
  // get their own graph node with no out-edges, exercising the SCC
  // analysis for non-recursive aliases mixed with a recursive cluster.
  // Map is omitted because the test fixture doesn't load collections.
  const char* src =
    "type JsonNull    is None\n"
    "type JsonBoolean is Bool\n"
    "type JsonString  is String\n"
    "type JsonNumber  is F64\n"
    "type JsonArray   is Array[JsonEntity]\n"

    "type JsonEntity is\n"
    "  ( JsonNull\n"
    "  | JsonBoolean\n"
    "  | JsonString\n"
    "  | JsonNumber\n"
    "  | JsonArray)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_COMPILE(src);
}




TEST_F(RecursiveTypeAliasTest, CompatTypeOnRecursiveAlias)
{
  // Forces is_compat_type to walk a recursive alias. Compatible/
  // incompatible cap pairing on a value typed by a recursive alias
  // recurses through the type's union members; the cycle base case
  // (return true) lets the analysis terminate.
  const char* src =
    "type R is (None | Array[R] val)\n"

    "primitive Probe\n"
    "  fun take(x: R val): None => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let r: R val = recover val None end\n"
    "    Probe.take(r)";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, SafeAutorecoverOnRecursiveAlias)
{
  // Forces safe_to_autorecover to walk a recursive alias. The
  // autorecover check applies during method-call typecheck on an
  // iso receiver whose method takes a recursive-alias arg; cycle
  // protection (return true on cycle) keeps the recursion finite.
  const char* src =
    "type R is (None | Array[R])\n"

    "class Holder\n"
    "  fun ref take(x: R): None => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let h: Holder iso = recover iso Holder end\n"
    "    h.take(None)\n"
    "    consume h";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, ApplyCapOnRecursiveAlias)
{
  // Forces apply_cap to walk a recursive alias. apply_cap is invoked
  // during typeparam capability rewriting; using a recursive alias
  // as a typearg routes through it. Cycle base case (return true) on
  // re-entry keeps the cap rewrite finite.
  const char* src =
    "type R is (None | Array[R])\n"

    "class Box[T: Any #read]\n"
    "  let value: T\n"
    "  new create(v: T) => value = v\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let r: R = None\n"
    "    let b: Box[R] = Box[R](r)";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, ConstraintContainsTupleOnRecursiveAlias)
{
  // Forces constraint_contains_tuple to walk a recursive alias.
  // Used in pass/flatten.c when examining a typeparam constraint
  // for tuple shapes; the cycle base case (return false) keeps the
  // walk terminating when the constraint references a recursive
  // alias whose body lists a tuple-typed nominal typearg.
  const char* src =
    "type R is (None | Array[R])\n"

    "class Holder[T: R #read]\n"
    "  let value: T\n"
    "  new create(v: T) => value = v\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let h: Holder[R] = Holder[R](None)";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, LongNonRecursiveAliasChainCompiles)
{
  // A bare alias chain of N links is legal Pony — there's no cycle,
  // PASS_TYPEALIAS_RECURSION accepts it. typealias_unfold must follow
  // the chain to its concrete tail without spurious failure regardless
  // of N.
  //
  // Regression coverage for two distinct bugs:
  // (1) A previous depth-counter implementation in typealias_unfold
  //     treated long non-cyclic chains as if they were cycles and
  //     aborted via pony_assert past 1024 hops.
  // (2) The iterative Tarjan in pass/typealias_recursion.c replaced a
  //     recursive form that blew the C stack at ~130k links on Linux
  //     with default 8 MB. 50k links exercises the iterative form well
  //     past the 1024 threshold and far enough into the recursive
  //     form's danger zone that a regression there would either crash
  //     here or come close enough that a modest bump would catch it.
  //     The test takes a handful of seconds in release builds (~10s
  //     observed; varies with the test machine).
  const size_t chain_len = 50000;

  std::string src;
  src.reserve(chain_len * 24);
  for(size_t i = 0; i < chain_len - 1; i++)
  {
    src += "type A";
    src += std::to_string(i);
    src += " is A";
    src += std::to_string(i + 1);
    src += "\n";
  }
  src += "type A";
  src += std::to_string(chain_len - 1);
  src += " is U64\n";

  src +=
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: A0 = 5\n"
    "    None";

  TEST_COMPILE(src.c_str());
}


TEST_F(RecursiveTypeAliasTest, DenseForwardDagInSccCompilesInBoundedTime)
{
  // Regression test for exponential-time DFS in the legality pass'
  // non-constructive cycle check (check_base_cases). Without 3-color
  // marking — a `done` set marked on pop, so already-explored subtrees
  // are skipped — the gray-only DFS re-explores every path from every
  // start, giving 2^V time on forward DAGs.
  //
  // Shape: a single SCC of N aliases. Each Ai has non-constructive
  // forward edges to A(i+1)..AN (an upper-triangular DAG of non-cons
  // edges) plus a constructive Array[A1] edge that pulls everything
  // into one SCC. The non-cons subgraph is acyclic, so the check must
  // accept — at N=32 the pre-fix DFS took ~52s standalone in the
  // legality pass alone, doubling every +2 increment in N.
  //
  // Bounded to PASS_TYPEALIAS_RECURSION because downstream passes
  // (expr in particular) do their own deep work on this many mutually-
  // recursive aliases that's unrelated to this regression.
  const size_t scc_size = 40;

  std::string src;
  for(size_t i = 1; i <= scc_size; i++)
  {
    src += "type A";
    src += std::to_string(i);
    src += " is (None | Array[A1]";
    for(size_t j = i + 1; j <= scc_size; j++)
    {
      src += " | A";
      src += std::to_string(j);
    }
    src += ")\n";
  }
  src +=
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  auto start = std::chrono::steady_clock::now();
  DO(test_compile(src.c_str(), "typealias_recursion"));
  auto elapsed = std::chrono::steady_clock::now() - start;

  auto seconds =
    std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
  ASSERT_LT(seconds, 15)
    << "N=" << scc_size << " dense-forward-DAG SCC legality pass took "
    << seconds << "s; expected <15s. Suggests check_base_cases' non-cons "
       "cycle DFS regressed to gray-only (no `done` set), re-exploring "
       "every path from every start in exponential time on forward DAGs.";
}


TEST_F(RecursiveTypeAliasTest, MutualSccCompilesInBoundedTime)
{
  // Regression test for compile-time DoS on small mutually-recursive
  // alias SCCs. A ring `type Ai is (None | Array[A((i+1) mod N)])`
  // forces the type checker to walk the SCC repeatedly while proving
  // is_subtype on the resulting Array nominals. Before is_assumption_match
  // was made structural in `src/libponyc/type/type_assume.c`, the matcher
  // re-entered the full subtype machinery via `is_eqtype` on every
  // typearg comparison; cost grew exponentially in N.
  //
  // Standalone ponyc measurements (release build, full stdlib) on the
  // same shape: N=3 ~32-39s; N=4 didn't finish in 90s; N=50 didn't
  // finish in 696s. The H3 brief's acceptance bar is N=4 in <5s
  // standalone; post-fix N=4 standalone is 1.1s. ThreeAliasMutualRecursion
  // already exercises a structurally-similar SCC-3 shape but doesn't
  // *use* the alias, so it never reaches the reach pass and didn't
  // catch the regression. This test uses `let x: A0 = None` to force
  // the reach pass to walk the SCC.
  //
  // The test framework loads less than the full stdlib, so absolute
  // timings are smaller than standalone runs. Post-fix N=3 here takes
  // tens of milliseconds; pre-fix it took ~35s in the framework. N=3
  // is used (not N=4) so the pre-fix run *completes* in time for the
  // ASSERT_LT below to fire, instead of hanging past CI's per-test
  // timeout — that gives a clean regression signal. The 30-second
  // budget gives ~600x headroom over post-fix runtime.
  //
  // For the related bounded-time regression on recursive *generic
  // interfaces* (drifting same-def chains, ponylang/ponyc#1216), see
  // BadPonyTest.RecursiveGenericInterfaceDoesNotHang in badpony.cc.
  // That shape is bounded by the divergence guard in is_x_sub_x
  // rather than by structural cycle detection in is_assumption_match.
  const size_t scc_size = 3;

  std::string src;
  for(size_t i = 0; i < scc_size; i++)
  {
    src += "type A";
    src += std::to_string(i);
    src += " is (None | Array[A";
    src += std::to_string((i + 1) % scc_size);
    src += "])\n";
  }
  src +=
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: A0 = None\n"
    "    None";

  auto start = std::chrono::steady_clock::now();
  TEST_COMPILE(src.c_str());
  auto elapsed = std::chrono::steady_clock::now() - start;

  auto seconds =
    std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
  ASSERT_LT(seconds, 30)
    << "SCC-" << scc_size << " compile took " << seconds
    << "s; expected <30s. Suggests is_assumption_match regressed to "
       "calling is_eqtype on typeargs (see type_assume.c).";
}


// Smoke coverage of cap-correct subtype outcomes on recursive aliases.
// is_assumption_match in type_assume.c intentionally omits cap and eph
// (the subtype_cache fingerprint includes them; the matcher does not),
// and these tests pin the user-visible compile outcomes for a few cap
// pairs against a recursive alias.
//
// What these tests don't do: they don't pin the matcher's mechanism.
// Patching is_assumption_match to be cap-aware produces the same
// compile outcomes on these inputs — the assumption stack rarely sees
// the same nominal/def queried at different cap pairs on these
// shapes, so the cap-blindness doesn't get to matter. Treat these as
// observational coverage that recursive aliases produce cap-correct
// compile outcomes through the typecheck path, not as a regression
// test for the cap-blind/cap-aware choice.

TEST_F(RecursiveTypeAliasTest, RecursiveAliasValToBoxCompiles)
{
  // val <: box. Passing a `Tree val` where `Tree box` is expected
  // must compile.
  const char* src =
    "type Tree is (None | Array[Tree])\n"

    "class Sink\n"
    "  fun take(t: Tree box) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let v: Tree val = None\n"
    "    Sink.take(v)";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, RecursiveAliasValToIsoRejected)
{
  // val is not <: iso. Passing a `Tree val` where `Tree iso` is
  // expected must reject.
  const char* src =
    "type Tree is (None | Array[Tree])\n"

    "class Sink\n"
    "  fun take(t: Tree iso) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let v: Tree val = None\n"
    "    Sink.take(v)";

  TEST_ERROR(src);
}


TEST_F(RecursiveTypeAliasTest, RecursiveAliasIsoToValRejected)
{
  // Aliasing an iso doesn't yield a val. Passing an aliased `Tree iso`
  // where `Tree val` is expected must reject.
  const char* src =
    "type Tree is (None | Array[Tree])\n"

    "class Sink\n"
    "  fun take(t: Tree val) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let i: Tree iso = recover iso Array[Tree] end\n"
    "    Sink.take(i)";

  TEST_ERROR(src);
}


TEST_F(RecursiveTypeAliasTest, RecursiveAliasRefToBoxCompiles)
{
  // ref <: box. Passing a `Tree ref` where `Tree box` is expected
  // must compile.
  const char* src =
    "type Tree is (None | Array[Tree])\n"

    "class Sink\n"
    "  fun take(t: Tree box) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let r: Tree ref = Array[Tree]\n"
    "    Sink.take(r)";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, ChainedRecursiveAliasValToBoxCompiles)
{
  // Deeper shape: the recursion threads through a chained alias to a
  // generic. Exercises the cycle stack across more frames than the
  // single-level Tree shape.
  const char* src =
    "type Wrap[A] is Array[A]\n"
    "type R is (None | Wrap[R])\n"

    "class Sink\n"
    "  fun take(t: R box) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let v: R val = None\n"
    "    Sink.take(v)";

  TEST_COMPILE(src);
}


TEST_F(RecursiveTypeAliasTest, ChainedRecursiveAliasValToIsoRejected)
{
  // Same chained shape, cap-incompatible direction.
  const char* src =
    "type Wrap[A] is Array[A]\n"
    "type R is (None | Wrap[R])\n"

    "class Sink\n"
    "  fun take(t: R iso) => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let v: R val = None\n"
    "    Sink.take(v)";

  TEST_ERROR(src);
}

