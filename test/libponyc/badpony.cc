#include <gtest/gtest.h>
#include <platform.h>

#include <chrono>
#include <cstring>

#include "util.h"


/** Pony code that parses, but is erroneous. Typically type check errors and
 * things used in invalid contexts.
 *
 * We build all the way up to and including code gen and check that we do not
 * assert, segfault, etc but that the build fails and at least one error is
 * reported.
 *
 * There is definite potential for overlap with other tests but this is also a
 * suitable location for tests which don't obviously belong anywhere else.
 */

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }

#define TEST_ERRORS_2(src, err1, err2) \
  { const char* errs[] = {err1, err2, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }

#define TEST_ERRORS_3(src, err1, err2, err3) \
  { const char* errs[] = {err1, err2, err3, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }

// Single error with its primary message plus a continuation frame.
// 'primary' and 'note' are matched as substrings.
#define TEST_ERROR_WITH_NOTE(src, primary, note) \
  { const char* errs[] = {primary, NULL}; \
    const char* frame_strs[] = {note, NULL}; \
    const char** frames[] = {frame_strs, NULL}; \
    DO(test_expected_error_frames(src, "ir", errs, frames)); }


class BadPonyTest : public PassTest
{};


// Cases from reported issues

TEST_F(BadPonyTest, ClassInOtherClassProvidesList)
{
  // From issue #218
  const char* src =
    "class Named\n"
    "class Dog is Named\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src, "invalid provides type. Can only be interfaces, traits and intersects of those.");
}

TEST_F(BadPonyTest, TypeParamMissingForTypeInProvidesList)
{
  // From issue #219
  const char* src =
    "trait Bar[A]\n"
    "  fun bar(a: A) =>\n"
    "    None\n"

    "trait Foo is Bar // here also should be a type argument, like Bar[U8]\n"
    "  fun foo() =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src, "not enough type arguments");
}

TEST_F(BadPonyTest, TupleIndexIsZero)
{
  // From issue #397
  const char* src =
    "primitive Foo\n"
    "  fun bar(): None =>\n"
    "    (None, None)._0";

  TEST_ERRORS_1(src, "Did you mean _1?");
}

TEST_F(BadPonyTest, TupleIndexIsOutOfRange)
{
  // From issue #397
  const char* src =
    "primitive Foo\n"
    "  fun bar(): None =>\n"
    "    (None, None)._3";

  TEST_ERRORS_1(src, "Valid range is [1, 2]");
}

TEST_F(BadPonyTest, InvalidLambdaReturnType)
{
  // From issue #828
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    {(): tag => this }\n";

  TEST_ERRORS_1(src, "lambda return type: tag");
}

TEST_F(BadPonyTest, InvalidMethodReturnType)
{
  // From issue #828
  const char* src =
    "primitive Foo\n"
    "  fun bar(): iso =>\n"
    "    U32(1)\n";

  TEST_ERRORS_1(src, "function return type: iso");
}

TEST_F(BadPonyTest, TypeErrorInFFIArguments)
{
  // From issue #2114
  const char *src =
    "use @foo[None](i: Pointer[U32])\n"
    "actor Main\n"
    "  fun create(env: Env) =>\n"
    "    @foo(addressof U32(0))";

  TEST_ERRORS_1(src, "can only take the address of a local, field or method");
}

TEST_F(BadPonyTest, ObjectLiteralUninitializedField)
{
  // From issue #879
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    object\n"
    "      let x: I32\n"
    "    end";

  TEST_ERRORS_1(src, "object literal fields must be initialized");
}

TEST_F(BadPonyTest, LambdaCaptureVariableBeforeDeclarationWithTypeInferenceExpressionFail)
{
  // From issue #1018
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {()(x) => None }\n"
    "    let x = 0";

   TEST_ERRORS_1(src, "declaration of 'x' appears after use");
}

// TODO: This test is not correct because it does not fail without the fix.
// I do not know how to generate a test that calls genheader().
// Comments are welcomed.
/*TEST_F(BadPonyTest, ExportedActorWithVariadicReturnTypeContainingNone)
{
  // From issue #891
  const char* src =
    "primitive T\n"
    "\n"
    "actor @A\n"
    "  fun f(a: T): (T | None) =>\n"
    "    a\n";

  TEST_COMPILE(src);
}*/

TEST_F(BadPonyTest, TypeAliasRecursionThroughTypeParameterInTuple)
{
  // From issue #901. The recursion has a constructive edge (Foo
  // appears inside Array's typearg position). The body is a tuple
  // around a union-free shape — there is no union anywhere reachable
  // from Foo's body to provide a non-recursive alternative, so the
  // legality pass rejects it for lack of a base case.
  const char* src =
    "type Foo is (Array[Foo], None)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive",
    "add a non-recursive alternative");
}


TEST_F(BadPonyTest, TypeAliasDirectUnionSelfReference)
{
  // type A is (A | None) — A appears directly as a union member of its
  // own body. The recursive arm has no constructor wrapping it (no
  // class, no nominal typearg position), so there's no constructive
  // edge in the cycle and the legality pass rejects.
  const char* src =
    "type A is (A | None)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "can't be infinitely recursive");
}


TEST_F(BadPonyTest, TypeAliasMutualThroughUnionMembers)
{
  // X and Y mutually recurse through union members only. Each cycle
  // edge is a bare alias reference inside a union — no constructor
  // wraps the recursion, so it's unproductive even though each union
  // has a non-recursive member. Pin the multi-alias variant of the
  // hint so a refactor that changes the wording is caught.
  const char* src =
    "type X is (Y | String)\n"
    "type Y is (X | U32)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  const char* errs[] = {"can't be infinitely recursive", NULL};
  const char* frame_strs[] = {
    "alias 'Y' is part of the same cycle",
    "the recursion must thread through a class's type argument "
    "(e.g., 'Array[<typearg>]'); recursion through union members or tuple "
    "elements has no finite layout",
    NULL};
  const char** frames[] = {frame_strs, NULL};
  test_expected_error_frames(src, "ir", errs, frames);
}


TEST_F(BadPonyTest, TypeAliasLargeCycleTruncatesPerAliasFramesSingular)
{
  // Smallest cycle that hides exactly one alias behind the cap.
  // scc_size = 5 with cap = 3 leaves 1 alias hidden, so the summary
  // frame must read "alias is" not "aliases are". Pins the singular
  // form so a future refactor that drops the singular branch is caught.
  const char* src =
    "type A1 is (None | A2)\n"
    "type A2 is (None | A3)\n"
    "type A3 is (None | A4)\n"
    "type A4 is (None | A5)\n"
    "type A5 is (None | A1)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  const char* errs[] = {"can't be infinitely recursive", NULL};
  const char* frame_strs[] = {
    "alias 'A2' is part of the same cycle",
    "alias 'A3' is part of the same cycle",
    "alias 'A4' is part of the same cycle",
    "(and 1 more alias is part of this cycle)",
    "the recursion must thread through a class's type argument "
    "(e.g., 'Array[<typearg>]'); recursion through union members or tuple "
    "elements has no finite layout",
    NULL};
  const char** frames[] = {frame_strs, NULL};
  test_expected_error_frames(src, "ir", errs, frames);
}


TEST_F(BadPonyTest, TypeAliasLargeCycleTruncatesPerAliasFrames)
{
  // Ten aliases in a cycle. Caps the per-alias "is part of the same
  // cycle" fan-out at three frames plus a summary frame counting the
  // remaining aliases. Beyond a handful, source pointers stop helping
  // the user diagnose the cycle, and ast_error_frame's tail-walking
  // append makes the unbounded loop O(N^2) — at scc_size = 50000 the
  // pre-cap loop took ~22s walking the frame chain. The cycle path in
  // the headline names every alias in order, so the per-alias frames
  // are redundant detail past the first few.
  //
  // Pin: A2..A4 each get a frame, A5..A9 do NOT, the summary frame
  // names the remaining count, and the suggestion frame still emits
  // last. Counterfactual: removing the cap restores the unbounded
  // fan-out and the frame count below would fail (10 aliases would
  // produce 9 alias frames + 1 suggestion = 10, not 4 + summary +
  // suggestion = 5).
  const char* src =
    "type A1 is (None | A2)\n"
    "type A2 is (None | A3)\n"
    "type A3 is (None | A4)\n"
    "type A4 is (None | A5)\n"
    "type A5 is (None | A6)\n"
    "type A6 is (None | A7)\n"
    "type A7 is (None | A8)\n"
    "type A8 is (None | A9)\n"
    "type A9 is (None | A10)\n"
    "type A10 is (None | A1)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  const char* errs[] = {"can't be infinitely recursive", NULL};
  const char* frame_strs[] = {
    "alias 'A2' is part of the same cycle",
    "alias 'A3' is part of the same cycle",
    "alias 'A4' is part of the same cycle",
    "(and 6 more aliases are part of this cycle)",
    "the recursion must thread through a class's type argument "
    "(e.g., 'Array[<typearg>]'); recursion through union members or tuple "
    "elements has no finite layout",
    NULL};
  const char** frames[] = {frame_strs, NULL};
  test_expected_error_frames(src, "ir", errs, frames);
}


TEST_F(BadPonyTest, TypeAliasRecursionThroughTuple)
{
  // type IntList is (None | (U32, IntList)) — IntList recurs through
  // a tuple element. Tuples are inline value types in Pony, so the
  // tuple's layout would have to inline another IntList, which would
  // have to inline another tuple, and so on without bound. There is
  // no finite layout. Recursion through a nominal's typeargs (Array[T]
  // etc.) is legal because nominal classes break the recursion at the
  // heap pointer.
  //
  // Pin the headline cycle suffix and the suggestion text with the
  // alias name interpolated so a reword of report_illegal_cycle is
  // caught.
  const char* src =
    "type IntList is (None | (U32, IntList))\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive (cycle: IntList -> IntList)",
    "the recursion must thread through a class's type argument, "
    "e.g., 'Array[IntList]'");
}

TEST_F(BadPonyTest, TypeAliasRecursionWithoutBaseCase)
{
  // type A is Array[A] — the cycle has a constructive edge (A appears
  // inside Array's typearg position, which is sound), but the alias
  // body is a bare nominal with no union to provide a non-recursive
  // alternative. Without a base case in any reachable union, the type
  // has no constructible inhabitant, so the legality pass rejects it.
  // Pin the suggestion text so a refactor that changes the hint
  // silently is caught.
  const char* src =
    "type A is Array[A]\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive",
    "add a non-recursive alternative in a union, e.g., "
    "'type A is (None | <body>)'");
}

TEST_F(BadPonyTest, TypeAliasWrapWithoutBaseCase)
{
  // Wrap[T] = Array[T] is non-recursive but has no union anywhere in
  // its body. X = Wrap[X] is rejected because the base-case walker
  // (which does follow cross-SCC alias references) finds no union
  // reachable from X's body, so there's no base case.
  const char* src =
    "type Wrap[T] is Array[T]\n"
    "type X is Wrap[X]\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "can't be infinitely recursive");
}

TEST_F(BadPonyTest, TypeAliasMutualThroughWrapWithoutBaseCase)
{
  // SCC {A, B} mutually recursive through a non-recursive Wrap that
  // has no union. Cross-SCC ref to Wrap, followed into Wrap's body,
  // finds Array[T] — no union, no base case. Should be rejected.
  // Pin the multi-alias base-case hint so a refactor that changes the
  // wording is caught.
  const char* src =
    "type Wrap[T] is Array[T]\n"
    "type A is Wrap[B]\n"
    "type B is Wrap[A]\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  const char* errs[] = {"can't be infinitely recursive", NULL};
  const char* frame_strs[] = {
    "alias 'B' is part of the same cycle",
    "add a non-recursive alternative in a union somewhere in the "
    "cycle (e.g., '(None | ...)') so the type has a base case",
    NULL};
  const char** frames[] = {frame_strs, NULL};
  test_expected_error_frames(src, "ir", errs, frames);
}

TEST_F(BadPonyTest, TypeAliasDirectSelfReference)
{
  // type A is A — bare self-reference at the alias body's top level.
  // No constructor, no union, no class wraps the recursion. The
  // legality pass rejects: there's no constructive edge in the
  // cycle. From the Phase 3 plan's TypeAliasDirectPositionSelfRecursion
  // case. Pin the suggestion text too so a refactor that changes the
  // hint silently is caught.
  const char* src =
    "type A is A\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive",
    "the recursion must thread through a class's type argument, "
    "e.g., 'Array[A]'");
}

TEST_F(BadPonyTest, TypeAliasIntersectionNotBaseCase)
{
  // type A is Array[(A & Any)] — A appears inside an intersection
  // inside Array's typearg position. The cycle has a constructive
  // edge (Array breaks the recursion at the heap), but no union is
  // reachable from A's body, so no base case exists. Confirms D1:
  // intersections don't offer a base-case escape.
  const char* src =
    "type A is Array[(A & Any)]\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive",
    "add a non-recursive alternative");
}

TEST_F(BadPonyTest, TypeAliasTupleInsideTypeargWithoutUnion)
{
  // type A is Array[(A, None)] — A appears inside a tuple inside
  // Array's typearg. The cycle has a constructive edge through
  // Array's typearg position (preserved through the tuple). But no
  // union is reachable, so no base case. Distinct from the
  // TypeAliasRecursionThroughTuple case where the tuple is at the
  // alias body's top level (no constructive edge).
  const char* src =
    "type A is Array[(A, None)]\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive",
    "add a non-recursive alternative");
}

TEST_F(BadPonyTest, TypeAliasConservativeRuleChangingTypeargs)
{
  // type A[T] is (T | B[T]); type B[T] is A[Array[T]].
  // A's body has a union with T as a member, B's body wraps T inside
  // Array. A future "let's analyze post-substitution" change might
  // try to reason about how T flows through and conclude the cycle
  // is fine, but the conservative rule (D3) treats the SCC's edges
  // as static: A -> B and B -> A both non-constructive (the
  // typealiasref appears at top-level positions, not inside a
  // nominal). No constructive edges in cycle, rejected.
  const char* src =
    "type A[T] is (T | B[T])\n"
    "type B[T] is A[Array[T]]\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  const char* errs[] = {"can't be infinitely recursive", NULL};
  const char* frame_strs[] = {
    "alias 'B' is part of the same cycle",
    "recursion must thread through",
    NULL};
  const char** frames[] = {frame_strs, NULL};
  test_expected_error_frames(src, "ir", errs, frames);
}

TEST_F(BadPonyTest, TypeAliasWrapLaundersMixedTypeparamUse)
{
  // type Wrap[T] is (T | Array[T] | None); type X is Wrap[X].
  // Wrap uses T both constructively (Array[T]) and non-constructively
  // (bare in union). The unfolded form is (X | Array[X] | None) which
  // accepts on its own, but laundering the same shape through Wrap
  // used to crash downstream codegen. The legality classification has
  // to take the worst case: a typeparam used non-constructively
  // anywhere in the wrap's body propagates non-constructively, even
  // if some other use is constructive.
  const char* src =
    "type Wrap[T] is (T | Array[T] | None)\n"
    "type X is Wrap[X]\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive",
    "recursion must thread through");
}

TEST_F(BadPonyTest, TypeAliasWrapLaundersBareTypeparamRecursion)
{
  // type Wrap[T] is (T | None); type X is Wrap[X]. The unfolded form
  // is (X | None) — bare X in a union with no constructor wrapping
  // it, semantically the same as type A is (A | None) which is
  // illegal. Without checking that the wrap alias actually uses its
  // typeparam at a constructive position, the legality pass would
  // accept this (the graph-level X -> X via Wrap's typearg looks
  // constructive). The fix walks Wrap's body to confirm T appears
  // inside a TK_NOMINAL's typeargs; here it doesn't, so X -> X is
  // reclassified as non-constructive and the cycle is rejected.
  const char* src =
    "type Wrap[T] is (T | None)\n"
    "type X is Wrap[X]\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive",
    "recursion must thread through");
}

TEST_F(BadPonyTest, TypeAliasWrapLaundersBareTypeparamUnderArrow)
{
  // type Wrap[T] is (None | (box->T)); type X is Wrap[X].
  // Wrap puts T at the RHS of an arrow viewpoint — that's a structural
  // occurrence (D5: only the LHS of an arrow is a viewpoint slot). Bare
  // typeparam at a non-constructive position; legality must reject.
  const char* src =
    "type Wrap[T] is (None | (box->T))\n"
    "type X is Wrap[X]\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive",
    "recursion must thread through");
}

TEST_F(BadPonyTest, TypeAliasWrapLaundersBareTypeparamInIntersection)
{
  // type Wrap[T] is (None | (T & Any tag)); type X is Wrap[X].
  // T appears as a member of an intersection — same shape as a union
  // member from a constructiveness standpoint. No constructor wraps
  // it, so the typeparam-use classifier must mark X non-constructive.
  const char* src =
    "type Wrap[T] is (None | (T & Any tag))\n"
    "type X is Wrap[X]\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive",
    "recursion must thread through");
}

TEST_F(BadPonyTest, TypeAliasWrapMultipleTypeparamsBareSlot)
{
  // type Wrap[T, U] is (T | Array[U] | None); type X is Wrap[X, U64].
  // U is used constructively (Array[U]); T is used non-constructively
  // (bare in union). Substituting X into T's slot threads X through a
  // bare position, so the cycle is non-constructive even though
  // another slot in Wrap is constructive.
  const char* src =
    "type Wrap[T, U] is (T | Array[U] | None)\n"
    "type X is Wrap[X, U64]\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive",
    "recursion must thread through");
}

TEST_F(BadPonyTest, TypeAliasWrapBaseCaseFalsePositiveTypeparamUnion)
{
  // type Wrap[T] is (T | T); type X is (Array[X] | Wrap[X]).
  // Without the inside-wrap-body flag in subtree_references_scc, the
  // legality walker would visit Wrap[X]'s body, see (T | T), classify
  // each member as a TK_TYPEPARAMREF (not in the SCC), and conclude
  // that Wrap supplies a base case. After substitution T = X, both
  // members ARE the SCC member, so there's no real base case and
  // codegen later crashes. The fix: when walking inside a non-SCC
  // alias's body, treat any TK_TYPEPARAMREF as referencing the SCC
  // because at the call site it could have been substituted with an
  // SCC member.
  const char* src =
    "type Wrap[T] is (T | T)\n"
    "type X is (Array[X] | Wrap[X])\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive",
    "recursion must thread through");
}

TEST_F(BadPonyTest, TypeAliasMixedConstructiveAndBareSelfReference)
{
  // type X is (X | None | Array[X]). The cycle has both a constructive
  // arm (Array[X], heap-broken) and a non-constructive arm (bare X at
  // top of union, no constructor). The bare arm makes typealias_unfold
  // loop downstream — picking that branch reproduces X with X still at
  // top. Pre-fix, the legality pass accepted because there was at
  // least one constructive intra-SCC edge; post-fix, it rejects
  // because the non-constructive intra-SCC subgraph contains a self-
  // loop X -> X. Other shapes that should still pass: 'type Y is
  // (None | Array[Y])' (no bare arm), and the cross-alias wrap shape
  // 'type J is (S | A); type A is Array[J]' (J -> A non-constructively,
  // but A -> J only constructively, so the non-constructive subgraph
  // has no cycle).
  const char* src =
    "type X is (X | None | Array[X])\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: X = None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive",
    "recursion must thread through");
}

TEST_F(BadPonyTest, TypeAliasArrowRhsRecursion)
{
  // type A is (box->A) — A appears as the RHS of a TK_ARROW. Per D5
  // arrow's LHS is a viewpoint (cap or typeparam), not a structural
  // occurrence; the RHS IS a structural occurrence. The legality
  // pass walks into the arrow's RHS only, finds A, records a
  // non-constructive edge. No constructive edge, rejected.
  const char* src =
    "type A is (box->A)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive",
    "recursion must thread through");
}

TEST_F(BadPonyTest, TypeAliasUnrelatedSiblingMasksNonConstructive)
{
  // Regression test for the visited-set leak in typeparam_use_in_body
  // (pass/typealias_recursion.c). The wrap-alias classifier walks a
  // union body looking for how each sibling propagates the outer
  // typeparam. An earlier implementation pushed each visited
  // TK_TYPEALIASREF def onto a scratch set and never popped, and its
  // typearg recursion also left aliases on visited across siblings.
  // The combined effect: a first sibling whose typearg doesn't mention
  // the outer typeparam (Foo[I32]) still pushed Foo onto visited; a
  // later sibling whose typearg referenced Foo (Bar[Foo[T]]) then hit
  // the leaked Foo and classified the typearg as TP_UNUSED rather
  // than TP_USED_NON_CONSTRUCTIVE. collect_edges skipped the slot,
  // the alias graph missed the A -> A non-constructive edge, and
  // 'type A is Outer[A]' compiled past the legality pass — its
  // expansion '(None2 | I32 | A)' is a bare self-referential union
  // with no finite layout, which then crashed the expr pass.
  const char* src =
    "primitive None2\n"
    "type Foo[X] is (X | None2)\n"
    "type Bar[Y] is (Y | None2)\n"
    "type Outer[T] is (Foo[I32] | Bar[Foo[T]])\n"
    "type A is Outer[A]\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive",
    "recursion must thread through");
}

TEST_F(BadPonyTest, TypeAliasIllegalCycleBadHint)
{
  // Bare self-reference under a union: type Bad is (Bad | None). The
  // recursive arm has no constructor wrapping it, so every value
  // would just be None. Pin the cycle suffix and the suggestion text
  // so a reword of report_illegal_cycle is caught.
  const char* src =
    "type Bad is (Bad | None)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive (cycle: Bad -> Bad)",
    "the recursion must thread through a class's type argument, "
    "e.g., 'Array[Bad]'");
}

TEST_F(BadPonyTest, TypeAliasIllegalCycleParametricNoConstructiveEdge)
{
  // Parametric variant of TypeAliasDirectSelfReference / the 'Bad'
  // shape: Bad[T] = (Bad[T] | None). The bare-name suggestion
  // 'Array[Bad]' wouldn't compile if pasted because Bad is parametric
  // — it needs T. Pin the parameterized form so a regression that
  // drops the typeparams again is caught.
  const char* src =
    "type Bad[T] is (Bad[T] | None)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive (cycle: Bad[T] -> Bad[T])",
    "the recursion must thread through a class's type argument, "
    "e.g., 'Array[Bad[T]]'");
}

TEST_F(BadPonyTest, TypeAliasIllegalCycleParametricNoBaseCase)
{
  // Parametric variant of TypeAliasRecursionWithoutBaseCase: Wrap[T]
  // is Array[Wrap[T]]. The bare-name suggestion 'type Wrap is ...'
  // wouldn't compile if pasted because Wrap is parametric — the
  // suggestion needs to include the [T] declaration. Pin the
  // parameterized form so a regression that drops the typeparams
  // again is caught.
  const char* src =
    "type Wrap[T] is Array[Wrap[T]]\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERROR_WITH_NOTE(src,
    "can't be infinitely recursive (cycle: Wrap[T] -> Wrap[T])",
    "add a non-recursive alternative in a union, e.g., "
    "'type Wrap[T] is (None | <body>)'");
}

TEST_F(BadPonyTest, ParenthesisedReturn)
{
  // From issue #1050
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    (return)";

  TEST_ERRORS_1(src, "use return only to exit early from a method");
}

TEST_F(BadPonyTest, ReturnError)
{
  // From issue #4934
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    _foo(true)\n"
    "\n"
    "  fun tag _foo(x: Bool): Bool =>\n"
    "    if x then return error end\n"
    "    x";

  TEST_ERRORS_1(src, "return value cannot be a control statement");
}

TEST_F(BadPonyTest, ParenthesisedReturn2)
{
  // From issue #1050
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo()\n"

    "  fun foo(): U64 =>\n"
    "    (return 0)\n"
    "    2";

  TEST_ERRORS_1(src, "Unreachable code");
}

TEST_F(BadPonyTest, MatchUncalledMethod)
{
  // From issue #903
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match foo\n"
    "    | None => None\n"
    "    end\n"

    "  fun foo() =>\n"
    "    None";

  TEST_ERRORS_2(src, "can't reference a method without calling it",
                     "this pattern can never match");
}

TEST_F(BadPonyTest, TupleFieldReassign)
{
  // From issue #1101
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var foo: (U64, String) = (42, \"foo\")\n"
    "    foo._2 = \"bar\"";

  TEST_ERRORS_1(src, "can't assign to an element of a tuple");
}

TEST_F(BadPonyTest, WithBlockTypeInference)
{
  // From issue #1135
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    with x = 1 do None end";

  TEST_ERRORS_3(src, "could not infer literal type, no valid types found",
                     "cannot infer type of $1$0",
                     "cannot infer type of x");
}

TEST_F(BadPonyTest, EmbedNestedTuple)
{
  // From issue #1136
  const char* src =
    "class Foo\n"
    "  fun get_foo(): Foo => Foo\n"

    "actor Main\n"
    "  embed foo: Foo\n"
    "  let x: U64\n"

    "  new create(env: Env) =>\n"
    "    (foo, x) = (Foo.get_foo(), 42)";

  TEST_ERRORS_1(src, "an embedded field must be assigned using a constructor");
}

TEST_F(BadPonyTest, CircularTypeInfer)
{
  // From issue #1334
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "let x = x.create()\n"
        "let y = y.create()";

  TEST_ERRORS_2(src,
    "can't use an undefined variable in an expression",
    "can't use an undefined variable in an expression");
}

TEST_F(BadPonyTest, CallConstructorOnTypeIntersection)
{
  // From issue #1398
  const char* src =
    "interface Foo\n"

    "type Isect is (None & Foo)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    Isect.create()";

  TEST_ERRORS_1(src, "can't call a constructor on a type intersection");
}

TEST_F(BadPonyTest, AssignToFieldOfIso)
{
  // From issue #1469
  const char* src =
    "class Foo\n"
    "  var x: String ref = String\n"
    "  fun iso bar(): String iso^ =>\n"
    "    let s = recover String end\n"
    "    x = s\n"
    "   consume s\n"

    "  fun ref foo(): String iso^ =>\n"
    "    let s = recover String end\n"
    "    let y: Foo iso = Foo\n"
    "    y.x = s\n"
    "    consume s";

  TEST_ERRORS_2(src,
    "right side must be a subtype of left side",
    "right side must be a subtype of left side"
    );
}

TEST_F(BadPonyTest, IndexArrayWithBrackets)
{
  // From issue #1493
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "let xs = [as I64: 1; 2; 3]\n"
        "xs[1]";

  TEST_ERRORS_1(src, "Value formal parameters not yet supported");
}

TEST_F(BadPonyTest, ShadowingBuiltinTypeParameter)
{
  const char* src =
    "class A[I8]\n"
      "let b: U8 = 0";

  TEST_ERRORS_1(src, "type parameter shadows existing type");
}

TEST_F(BadPonyTest, ShadowingTypeParameterInSameFile)
{
  const char* src =
    "trait B\n"
    "class A[B]";

  TEST_ERRORS_1(src, "can't reuse name 'B'");
}

TEST_F(BadPonyTest, TupleToUnionGentrace)
{
  // From issue #1561
  const char* src =
    "primitive X\n"
    "primitive Y\n"

    "class iso T\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    this((T, Y))\n"

    "  be apply(m: (X | (T, Y))) => None";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, RefCapViolationViaCapReadTypeParameter)
{
  // From issue #1328
  const char* src =
    "class Foo\n"
      "var i: USize = 0\n"
      "fun ref boom() => i = 3\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let a: Foo val = Foo\n"
        "call_boom[Foo val](a)\n"

      "fun call_boom[A: Foo #read](x: A) =>\n"
        "x.boom()";

  TEST_ERRORS_1(src, "receiver type is not a subtype of target type");
}

TEST_F(BadPonyTest, RefCapViolationViaCapAnyTypeParameter)
{
  // From issue #1328
  const char* src =
    "class Foo\n"
      "var i: USize = 0\n"
      "fun ref boom() => i = 3\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let a: Foo val = Foo\n"
        "call_boom[Foo val](a)\n"

      "fun call_boom[A: Foo #any](x: A) =>\n"
        "x.boom()";

  TEST_ERRORS_1(src, "receiver type is not a subtype of target type");
}

TEST_F(BadPonyTest, AliasAny)
{
  // Safeguard ensuring #alias <= #any not allowed by
  // is_cap_sub_cap_bound, related to PR #3643
  const char* src =
    "class Foo\n"
    "actor Main\n"
      "new create(env: Env) =>\n"
        "let a: Foo val = Foo\n"
        "alias_bound[Foo val](a)\n"

      "fun alias_bound[A](x: A) =>\n"
        "let x': A = x";

  TEST_ERRORS_1(src, "right side must be a subtype of left side");
}

TEST_F(BadPonyTest, ReturnUnmovedIso)
{
  // Issue #1964
  const char* src =
    "class Bar\n"
    "class Foo\n"
      "let x: Bar iso = Bar\n"
      "fun get_bad(): Bar val =>\n"
        "x\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "None\n";

  TEST_ERRORS_1(src, "function body isn't the result type");
}

TEST_F(BadPonyTest, TypeParamArrowClass)
{
  // From issue #1687
  const char* src =
    "class C1\n"

    "trait Test[A]\n"
    "  fun foo(a: A): A->C1\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, ArrowTypeParamInTypeConstraint)
{
  // From issue #1694
  const char* src =
    "trait T1[A: B->A, B]\n"
    "trait T2[A: box->B, B]";

  TEST_ERRORS_2(src,
    "arrow types can't be used as type constraints",
    "arrow types can't be used as type constraints");
}

TEST_F(BadPonyTest, ArrowTypeParamInMethodConstraint)
{
  // From issue #1809
  const char* src =
    "class Foo\n"
    "  fun foo[X: box->Y, Y](x: X) => None";

  TEST_ERRORS_1(src,
    "arrow types can't be used as type constraints");
}

TEST_F(BadPonyTest, AnnotatedIfClause)
{
  // From issue #1751
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    if \\likely\\ U32(1) == 1 then\n"
    "      None\n"
    "    end\n";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, CapSubtypeInConstrainSubtyping)
{
  // From PR #1816
  const char* src =
    "trait T\n"
    "  fun alias[X: Any iso](x: X!): X^\n"
    "class C is T\n"
    "  fun alias[X: Any tag](x: X!): X^ => x\n";

  TEST_ERRORS_1(src,
    "type does not implement its provides list");
}

TEST_F(BadPonyTest, AliasedTypeParamNotSubtypeOfUnaliased)
{
  // From issue #1798
  // X! should not be a subtype of ref->X: this would allow duplicating iso
  // references by taking an aliased (tag) reference and returning it as iso.
  const char* src =
    "class Foo\n"
    "  fun alias[X](x: X!) : X^ =>\n"
    "    let y : ref->X = consume x\n"
    "    consume y\n";

  TEST_ERRORS_1(src, "right side must be a subtype of left side");
}

TEST_F(BadPonyTest, ObjectInheritsLaterTraitMethodWithParameter)
{
  // From issue #1715
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    object is T end\n"

    "trait T\n"
    "  fun apply(n: I32): Bool =>\n"
    "    n == 0\n";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, AddressofMissingTypearg)
{
  const char* src =
    "use @foo[None](fn: Pointer[None] tag)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo(addressof fn)\n"

    "  fun fn[A]() => None";

  TEST_ERRORS_1(src,
    "not enough type arguments");
}

TEST_F(BadPonyTest, ThisDotFieldRef)
{
  // From issue #1865
  const char* src =
    "actor Main\n"
    "  let f: U8\n"
    "  new create(env: Env) =>\n"
    "    this.f = 1\n";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, CapSetInConstraintTypeParam)
{
  const char* src =
    "class A[X]\n"
    "class B[X: A[Any #read]]\n";

  TEST_ERRORS_1(src,
    "a capability set can only appear in a type constraint");
}

TEST_F(BadPonyTest, MatchCasePatternConstructorTooFewArguments)
{
  const char* src =
    "class C\n"
    "  new create(key: String) => None\n"

    "primitive Foo\n"
    "  fun apply(c: (C | None)) =>\n"
    "    match c\n"
    "    | C => None\n"
    "    end";

  TEST_ERRORS_1(src, "not enough arguments");
}

TEST_F(BadPonyTest, ThisDotWhereDefIsntInTheTrait)
{
  // From issue #1878
  const char* src =
    "trait T\n"
    "  fun foo(): USize => this.u\n"

    "class C is T\n"
    "  var u: USize = 0\n";

  TEST_ERRORS_1(src,
    "can't find declaration of 'u'");
}

TEST_F(BadPonyTest, DontCareTypeInTupleTypeOfIfBlockValueUnused)
{
  // From issue #1896
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    if true then\n"
    "      (var a, let _) = test()\n"
    "    end\n"
    "  fun test(): (U32, U32) =>\n"
    "    (1, 2)\n";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, ExhaustiveMatchCasesJumpAway)
{
  // From issue #1898
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    if true then\n"
    "      match env\n"
    "      | let env': Env => return\n"
    "      end\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, CallArgTypeErrorInsideTuple)
{
  // From issue #1895
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    (\"\", foo([\"\"]))\n"
    "  fun foo(x: Array[USize]) => None";

  TEST_ERRORS_1(src, "array element not a subtype of specified array type");
}

TEST_F(BadPonyTest, NonExistFieldReferenceInConstructor)
{
  // From issue #1932
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    this.x = None";

  TEST_ERRORS_2(src,
    "can't find declaration of 'x'",
    "left side must be something that can be assigned to");
}

TEST_F(BadPonyTest, TypeArgErrorInsideReturn)
{
  const char* src =
    "primitive P[A]\n"

    "primitive Foo\n"
    "  fun apply(): (P[None], U8) =>\n"
    "    if true then\n"
    "      return (P, 0)\n"
    "    end\n"
    "    (P[None], 1)";

  TEST_ERRORS_1(src, "not enough type arguments");
}

TEST_F(BadPonyTest, FieldReferenceInDefaultArgument)
{
  const char* src =
    "actor Main\n"
    "  let _env: Env\n"
    "  new create(env: Env) =>\n"
    "    _env = env\n"
    "    foo()\n"
    "  fun foo(env: Env = _env) =>\n"
    "    None";

  TEST_ERRORS_1(src, "can't reference 'this' in a default argument");
}

TEST_F(BadPonyTest, DefaultArgScope)
{
  const char* src =
    "actor A\n"
    "  fun foo(x: None = (let y = None; y)) =>\n"
    "    y";

  TEST_ERRORS_1(src, "can't find declaration of 'y'");
}

TEST_F(BadPonyTest, GenericMain)
{
  const char* src =
    "actor Main[X]\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "the Main actor cannot have type parameters");
}

TEST_F(BadPonyTest, LambdaParamNoType)
{
  // From issue #2010
  const char* src =
    "actor Main\n"
    "  new create(e: Env) =>\n"
    "    {(x: USize, y): USize => x }";

  TEST_ERRORS_1(src, "a lambda parameter must specify a type");
}

TEST_F(BadPonyTest, AsBadCap)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let v: I64 = 3\n"
    "    try (v as I64 iso) end";

  TEST_ERRORS_1(src, "this capture violates capabilities");
}

TEST_F(BadPonyTest, AsUnaliased)
{
  const char* src =
    "class trn Foo\n"

    "actor Main\n"
    "  let foo: (Foo|None) = Foo\n"

    "  new create(env: Env) => None\n"

    "  fun ref apply() ? =>\n"
    "    (foo as Foo trn)";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, CodegenMangledFunptr)
{
  // Test that we don't crash in codegen when generating the function pointer.
  const char* src =
    "use @foo[None](fn: Pointer[None] tag)\n"

    "interface I\n"
    "  fun foo(x: U32)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let i: I = this\n"
    "    @foo(addressof i.foo)\n"

    "  fun foo(x: Any) => None";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, AsFromUninferredReference)
{
  // From issue #2035
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let b = apply(true)\n"
    "    b as Bool\n"

    "  fun ref apply(s: String): (Bool | None) =>\n"
    "    true";

  TEST_ERRORS_2(src,
    "argument not assignable to parameter",
    "cannot infer type of b");
}

TEST_F(BadPonyTest, FFIDeclaredTupleArgument)
{
  const char* src =
    "use @foo[None](x: (U8, U8))\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo((0, 0))";

  TEST_ERRORS_1(src, "cannot pass tuples as FFI arguments");
}

TEST_F(BadPonyTest, FFIDeclaredTupleReturn)
{
  const char* src =
    "use @foo[(U8, U8)]()\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo()";

  TEST_ERRORS_1(src, "an FFI function cannot return a tuple");
}

TEST_F(BadPonyTest, FFICallInDefaultInterfaceFun)
{
  const char* src =
    "use @foo[None]()\n"

    "interface Foo\n"
    "  fun apply() =>\n"
    "    @foo()\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "Can't call an FFI function in a default method or behavior");
}

TEST_F(BadPonyTest, FFICallInDefaultInterfaceBe)
{
  const char* src =
    "use @foo[None]()\n"

    "interface Foo\n"
    "  be apply() =>\n"
    "    @foo()\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "Can't call an FFI function in a default method or behavior");
}

TEST_F(BadPonyTest, FFICallInDefaultTraitFun)
{
  const char* src =
    "use @foo[None]()\n"

    "trait Foo\n"
    "  fun apply() =>\n"
    "    @foo()\n"

    "actor Main is Foo\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "Can't call an FFI function in a default method or behavior");
}

TEST_F(BadPonyTest, FFICallInDefaultTraitBe)
{
  const char* src =
    "use @foo[None]()\n"

    "trait Foo\n"
    "  be apply() =>\n"
    "    @foo()\n"

    "actor Main is Foo\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "Can't call an FFI function in a default method or behavior");
}

TEST_F(BadPonyTest, FFIDeclaredTupleReturnAtCallSite)
{
  const char* src =
    "use @foo[None]()\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo[(U8, U8)]()";

  TEST_ERRORS_1(src, "an FFI function cannot return a tuple");
}

TEST_F(BadPonyTest, MatchExhaustiveLastCaseUnionSubset)
{
  // From issue #2048
  const char* src =
    "primitive P1\n"
    "primitive P2\n"

    "actor Main\n"
    "  new create(env: Env) => apply(None)\n"

    "  fun apply(p': (P1 | P2 | None)) =>\n"
    "    match p'\n"
    "    | None => None\n"
    "    | let p: (P1 | P2) => None\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, QuoteCommentsEmptyLine)
{
  // From issue #2027
  const char* src =
    "\"\"\"\n"
    "\"\"\"\n"
    "actor Main\n"
    "  new create(env: Env) => None\n";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, AsUninferredNumericLiteral)
{
  // From issue #2037
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    0 as I64\n"
    "    0.0 as F64";

  TEST_ERRORS_2(src,
    "Cannot cast uninferred numeric literal",
    "Cannot cast uninferred numeric literal");
}

TEST_F(BadPonyTest, AsNestedUninferredNumericLiteral)
{
  // From issue #2037
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    (0, 1) as (I64, I64)";

  TEST_ERRORS_1(src, "Cannot cast uninferred literal");
}

TEST_F(BadPonyTest, DontCareUnusedBranchValue)
{
  // From issue #2073
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    if false then\n"
    "      None\n"
    "    else\n"
    "      _\n"
    "    end";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, ForwardTuple)
{
  // From issue #2097
  const char* src =
    "class val X\n"

    "trait T\n"
    "  fun f(): Any val\n"

    "class C is T\n"
    "  fun f(): (X, USize) => (X, 0)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let t: T = C\n"
    "    t.f()";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, CoerceUninferredNumericLiteralThroughTypeArgWithViewpoint)
{
  // From issue #2181
  const char* src =
    "class A[T]\n"
    "  new create(x: T) => None\n"

    "primitive B[T]\n"
    "  fun apply(): A[T] =>\n"
    "    A[this->T](1)\n";

  TEST_ERRORS_1(src, "could not infer literal type, no valid types found");
}

TEST_F(BadPonyTest, UnionTypeMethodsWithDifferentParamNamesNamedParams)
{
  // From issue #394
  // disallow calling a method on a union with named arguments
  // when the parameter names differ
  const char* src =
    "primitive A\n"
    "  fun foo(a: U64, b: U64): U64 => a\n"
    "  fun bar(x: U64, y: U64): U64 => x\n\n"

    "primitive B\n"
    "  fun foo(b: U64, c: U64): U64 => b\n"
    "  fun bar(x: U64, y: U64): U64 => x\n\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let aba = add_ab(A)\n"
    "    let abb = add_ab(B)\n"
    "    let bca = add_bc(A)\n"
    "    let bcb = add_bc(B)\n"
    "    let xya = add_xy(A)\n"
    "    let xyb = add_xy(B)\n\n"

    "  fun add_ab(uni: (A | B)): U64 =>\n"
    "    uni.foo(where a = 80, b = 8)\n\n"

    "  fun add_bc(uni: (A | B)): U64 =>\n"
    "    uni.foo(where b = 80, c = 8)\n\n"

    "  fun add_xy(uni: (A | B)): U64 =>\n"
    "    uni.bar(where x = 80, y = 8)\n\n";

    TEST_ERRORS_2(src,
      "methods of this union type have different parameter names",
      "methods of this union type have different parameter names");
}

TEST_F(BadPonyTest, UnionTypeMethodsWithDifferentParamNamesPositional)
{
  // From issue #394
  // allow calling a method on a union with positional arguments
  // when the parameter names differ
  const char* src =
  "primitive A\n"
  "  fun foo(a: U64, b: U64): U64 => a\n"
  "  fun bar(x: U64, y: U64): U64 => x\n\n"

  "primitive B\n"
  "  fun foo(b: U64, c: U64): U64 => b\n"
  "  fun bar(x: U64, y: U64): U64 => x\n\n"

  "actor Main\n"
  "  new create(env: Env) =>\n"
  "    let aba = add_ab(A)\n"
  "    let abb = add_ab(B)\n"
  "    let bca = add_bc(A)\n"
  "    let bcb = add_bc(B)\n"
  "    let xya = add_xy(A)\n"
  "    let xyb = add_xy(B)\n\n"

  "  fun add_ab(uni: (A | B)): U64 =>\n"
  "    uni.foo(80, 8)\n\n"

  "  fun add_bc(uni: (A | B)): U64 =>\n"
  "    uni.foo(80, 8)\n\n"

  "  fun add_xy(uni: (A | B)): U64 =>\n"
  "    uni.bar(80, 8)\n\n";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, ApplySugarInferredLambdaArgument)
{
  // From issue #2233
  const char* src =
    "primitive Callable\n"
    "  fun apply(x: U64, fn: {(U64): U64} val): U64 => fn(x)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let callable = Callable\n"
    "    callable(0, {(x) => x + 1 })";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, IsComparingCreateSugar)
{
  // From issue #2024
  const char* src =
    "primitive P\n"
    "class C\n"
    "actor A\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    // invalid
    "    C is C\n"
    "    A is A\n"
    "    C is P\n"
    "    P is C\n"
    "    P is (P, C, P)\n"
    "    P is (P; C)\n"
    "    P is (P, ((C), P))\n"
    "    C isnt C\n"
    "    C isnt P\n"
    "    P isnt C\n"
    "    P isnt (P, C, P)\n"
    "    P isnt (P; C)\n"
    "    P isnt (P, ((C), P))\n"
    // valid
    "    P is P\n"
    "    P is (C; P)\n"
    "    P isnt P\n"
    "    P isnt (C; P)";
  {
    const char* err = "identity comparison with a new object will always be false";
    const char* errs[] = {err, err, err, err, err, err, err, err, err, err, err, err, err, NULL};
    DO(test_expected_errors(src, "verify", errs));
  }
}

TEST_F(BadPonyTest, IsComparingCreate)
{
  // From issue #4162, this is just the desugared
  // version of the previous test.
  const char* src =
    "primitive P\n"
    "class C\n"
    "actor A\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    // invalid
    "    C.create() is C.create()\n"
    "    A.create() is A.create()\n"
    "    C.create() is P.create()\n"
    "    P.create() is C.create()\n"
    "    P.create() is (P.create(), C.create(), P.create())\n"
    "    P.create() is (P.create(); C.create())\n"
    "    P.create() is (P.create(), ((C.create()), P.create()))\n"
    "    C.create() isnt C.create()\n"
    "    C.create() isnt P.create()\n"
    "    P.create() isnt C.create()\n"
    "    P.create() isnt (P.create(), C.create(), P.create())\n"
    "    P.create() isnt (P.create(); C.create())\n"
    "    P.create() isnt (P.create(), ((C.create()), P.create()))\n"
    // valid
    "    P.create() is P.create()\n"
    "    P.create() is (C.create(); P.create())\n"
    "    P.create() isnt P.create()\n"
    "    P.create() isnt (C.create(); P.create())";
  {
    const char* err = "identity comparison with a new object will always be false";
    const char* errs[] = {err, err, err, err, err, err, err, err, err, err, err, err, err, NULL};
    DO(test_expected_errors(src, "verify", errs));
  }
}

TEST_F(BadPonyTest, IsComparingNamedConstructor)
{
  // From issue #4162, this is about testing named
  // constructors, and not just the default one.
  const char* src =
    "primitive P\n"
    "\n"
    "class C\n"
    "  new create() => None\n"
    "  new testing() => None\n"
    "\n"
    "actor A\n"
    "  new create() => None\n"
    "  new testing() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    // invalid
    "    C.create() is C.testing()\n"
    "    A.create() is A.testing()\n"
    "    C.create() is P.create()\n"
    "    P.create() is C.testing()\n"
    "    P.create() is (P.create(), C.testing(), P.create())\n"
    "    P.create() is (P.create(); C.testing())\n"
    "    P.create() is (P.create(), ((C.testing()), P.create()))\n"
    "    C.create() isnt C.testing()\n"
    "    C.create() isnt P.create()\n"
    "    P.create() isnt C.testing()\n"
    "    P.create() isnt (P.create(), C.testing(), P.create())\n"
    "    P.create() isnt (P.create(); C.testing())\n"
    "    P.create() isnt (P.create(), ((C.testing()), P.create()))\n"
    // valid
    "    P.create() is P.create()\n"
    "    P.create() is (C.testing(); P.create())\n"
    "    P.create() isnt P.create()\n"
    "    P.create() isnt (C.testing(); P.create())";
  {
    const char* err = "identity comparison with a new object will always be false";
    const char* errs[] = {err, err, err, err, err, err, err, err, err, err, err, err, err, NULL};
    DO(test_expected_errors(src, "verify", errs));
  }
}

TEST_F(BadPonyTest, TypeErrorDuringArrayLiteralInference)
{
  // From issue 2602
  const char* src =
    "class C\n"
    "trait X\n"
    "class ExpectX[T: X]\n"
    "  fun trigger(arg: Iterator[T]) =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    ExpectX[C].trigger([C; C].values())";
  TEST_ERRORS_1(src, "type argument is outside its constraint");
}

// From issue #1977. Identity comparisons between two distinct concrete
// entity types can never be true, so reject them at compile time. Scope is
// deliberately limited to TK_NOMINAL operands bound to concrete entity
// definitions (class, actor, primitive, struct); unions, tuples, type
// parameters, and trait/interface operands are intentionally not flagged.
TEST_F(BadPonyTest, IsBetweenDifferentClasses)
{
  const char* src =
    "class C\n"
    "class D\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let c: C = C\n"
    "    let d: D = D\n"
    "    if c is d then None end";

  TEST_ERRORS_1(src,
    "identity comparison between disjoint concrete types");
}

TEST_F(BadPonyTest, IsntBetweenDifferentClasses)
{
  const char* src =
    "class C\n"
    "class D\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let c: C = C\n"
    "    let d: D = D\n"
    "    if c isnt d then None end";

  TEST_ERRORS_1(src,
    "identity comparison between disjoint concrete types");
}

TEST_F(BadPonyTest, IsBetweenClassAndActor)
{
  const char* src =
    "class C\n"
    "actor A\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let c: C = C\n"
    "    let a: A = A\n"
    "    if c is a then None end";

  TEST_ERRORS_1(src,
    "identity comparison between disjoint concrete types");
}

TEST_F(BadPonyTest, IsBetweenClassAndPrimitive)
{
  const char* src =
    "class C\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let c: C = C\n"
    "    let n: None = None\n"
    "    if c is n then None end";

  TEST_ERRORS_1(src,
    "identity comparison between disjoint concrete types");
}

TEST_F(BadPonyTest, IsBetweenDifferentPrimitives)
{
  const char* src =
    "primitive P\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let p: P = P\n"
    "    let n: None = None\n"
    "    if p is n then None end";

  TEST_ERRORS_1(src,
    "identity comparison between disjoint concrete types");
}

TEST_F(BadPonyTest, IsBetweenClassAndStruct)
{
  const char* src =
    "class C\n"
    "struct S\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let c: C = C\n"
    "    let s: S = S\n"
    "    if c is s then None end";

  TEST_ERRORS_1(src,
    "identity comparison between disjoint concrete types");
}

TEST_F(BadPonyTest, IsBetweenDifferentNumericPrimitives)
{
  // Machine-word numeric types (U8/U16/.../F64) are concrete primitives, so
  // cross-width identity comparison is statically disjoint and rejected.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    if U32(0) isnt U64(0) then None end";

  TEST_ERRORS_1(src,
    "identity comparison between disjoint concrete types");
}

TEST_F(BadPonyTest, IsBetweenDifferentActors)
{
  const char* src =
    "actor A\n"
    "actor B\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a: A = A\n"
    "    let b: B = B\n"
    "    if a is b then None end";

  TEST_ERRORS_1(src,
    "identity comparison between disjoint concrete types");
}

TEST_F(BadPonyTest, IsBetweenDisjointConcreteTypesViaAlias)
{
  // Both operands have alias types. The check must unwrap the aliases
  // before comparing definitions; otherwise the error would not fire.
  const char* src =
    "class C\n"
    "class D\n"
    "type CA is C\n"
    "type DA is D\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let ca: CA = C\n"
    "    let da: DA = D\n"
    "    if ca is da then None end";

  TEST_ERRORS_1(src,
    "identity comparison between disjoint concrete types");
}

TEST_F(BadPonyTest, IsBetweenDisjointFieldsThroughThisArrow)
{
  // Field reads inside a method body produce viewpoint-adapted types like
  // `this->C`. The check must unwrap the arrow before classifying the
  // nominal underneath.
  const char* src =
    "class C\n"
    "class D\n"
    "class Holder\n"
    "  let c: C = C\n"
    "  let d: D = D\n"
    "  fun ref check(): Bool => c is d\n"
    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src,
    "identity comparison between disjoint concrete types");
}

TEST_F(BadPonyTest, IsBetweenDisjointClassesViaSugar)
{
  // Pony desugars a bare type name in expression position to a default
  // constructor call (`C` -> `C.create()`). The existing "new object"
  // diagnostic in verify_is_comparand fires first; the && short-circuit in
  // verify_is suppresses the new disjoint-concrete check so the user sees
  // exactly one error per identity comparison, not two.
  const char* src =
    "class C\n"
    "class D\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    if C is D then None end";

  TEST_ERRORS_1(src,
    "identity comparison with a new object will always be false");
}

TEST_F(BadPonyTest, IsBetweenDistinctObjectLiterals)
{
  // Per the design decision in #1977: each `object end` literal synthesizes
  // a distinct anonymous class, so two literals are statically disjoint
  // concrete types. The comparison can never be true and is rejected.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a = object end\n"
    "    let b = object end\n"
    "    if a is b then None end";

  TEST_ERRORS_1(src,
    "identity comparison between disjoint concrete types");
}

TEST_F(BadPonyTest, NosupertypeAnnotationProvides)
{
  const char* src =
    "trait T\n"

    "primitive \\nosupertype\\ P is T";

  TEST_ERRORS_1(src, "a 'nosupertype' type cannot specify a provides list");
}

TEST_F(BadPonyTest, ThisViewpointWithIsoReceiver)
{
  // From issue #1887
  const char* src =
    "class A\n"
    "class Revealer\n"
    "  fun box reveal(x: this->A ref): A box => x\n"

    "actor Main\n"
    "new create(env: Env) =>\n"
    "  let revealer : Revealer iso = Revealer.create()\n"
    "  let opaque : A tag = A.create()\n"
    "  let not_opaque : A box = (consume revealer).reveal(opaque)\n";

  TEST_ERRORS_1(src, "argument not assignable to parameter");
}

TEST_F(BadPonyTest, DisallowPointerAndMaybePointerInEmbeddedType)
{
  // From issue #2596
  const char* src =
    "struct Ok\n"

    "class Whoops\n"
    "  embed ok: Ok = Ok\n"
    "  embed not_ok: Pointer[None] = Pointer[None]\n"
    "  embed also_not_ok: NullablePointer[Ok] = NullablePointer[Ok](Ok)\n"

    "actor Main\n"
    "new create(env: Env) =>\n"
    "  Whoops";

  TEST_ERRORS_2(src,
    "embedded fields must be classes or structs",
    "embedded fields must be classes or structs")
}

TEST_F(BadPonyTest, AllowAliasForNonEphemeralReturn)
{
  // Direct field reads return the viewpoint-adapted type (this->A)
  // without auto-aliasing, so the return type matches.
  const char* src =
    "class iso Inner\n"
    "  new iso create() => None\n"

    "class Container[A: Inner #any]\n"
    "  var inner: A\n"
    "  new create(inner': A) => inner = consume inner'\n"
    "  fun get_1(): this->A => inner\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let o = Container[Inner iso](Inner)\n"
    "    let i_1 : Inner tag = o.get_1()";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, AliasedFieldReadNotSubtypeOfViewpointReturn)
{
  // Reading a field into a local auto-aliases: the local has type this->A!.
  // Consuming the local doesn't strip the alias marker. The resulting
  // this->A! is not a subtype of this->A because for some instantiations
  // (e.g. A=iso, receiver=ref) the aliased cap (tag) is not a subcap of
  // the original (iso). See #1798.
  const char* src =
    "class iso Inner\n"
    "  new iso create() => None\n"

    "class Container[A: Inner #any]\n"
    "  var inner: A\n"
    "  new create(inner': A) => inner = consume inner'\n"
    "  fun get_2(): this->A => let tmp = inner; consume tmp\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let o = Container[Inner iso](Inner)\n"
    "    let i_2 : Inner tag = o.get_2()";

  TEST_ERRORS_1(src, "function body isn't the result type");
}

TEST_F(BadPonyTest, AllowNestedTupleAccess)
{
  // From issue #3354
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "        let x = (1, (2, 3))._2._1";

  TEST_ERRORS_1(src,
    "Cannot look up member _2 on a literal")
}

TEST_F(BadPonyTest, InterfacesCantHavePrivateMethods)
{
  // From issue #2287
  const char* src =
    "interface Foo\n"
    "  fun ref _set(i: USize, value: U8): U8";

  TEST_ERRORS_1(src,
    "interfaces can't have private methods, only traits can")
}

TEST_F(BadPonyTest, CantAssignErrorExpression)
{
  // From issue #3823
  const char* src =
    "actor Main\n"
      "new create(env: Env) =>\n"
        "let x: String = (error)";

  TEST_ERRORS_1(src,
    "right side must be something that can be assigned")
}

TEST_F(BadPonyTest, NotSafeToWrite)
{
  // From issue #4290
  const char* src =
    "class Foo\n"
    "class X\n"
      "var foo: Foo ref = Foo\n"

    "actor Main\n"
      "new create(env: Env) =>\n"
        "let x = X\n"
        "let foo: Foo ref = Foo\n"
        "x.foo = foo";

  {
      const char* errs[] =
        {"not safe to write right side to left side", NULL};
      const char* frame1[] = {
        "right side type: Foo ref^",
        "left side type: X iso", NULL};
      const char** frames[] = {frame1, NULL};
      DO(test_expected_error_frames(src, "badpony", errs, frames));
  }
}

TEST_F(BadPonyTest, MatchIsoFieldWithoutConsume)
{
  // From issue #4579
  const char* src =
    "class Bad\n"
    "  var _s: String iso\n"

    "  new iso create(s: String iso) =>\n"
    "    _s = consume s\n"

    "  fun ref take(): String iso^ =>\n"
    "    match _s\n"
    "    | let s': String iso => consume s'\n"
    "    end";

    TEST_ERRORS_1(src, "this capture violates capabilities");
}

TEST_F(BadPonyTest, MatchIsoLetWithoutConsume)
{
  // From issue #4579
  const char* src =
    "class Bad\n"
    "  fun bad() =>\n"
    "    let a: String iso = recover iso String end\n"

    "    match a\n"
    "    | let a': String iso => None\n"
    "    end";

    TEST_ERRORS_1(src, "this capture violates capabilities");
}

TEST_F(BadPonyTest, AssignToEphemeralCapability)
{
  // From issue #4344
  // Assigning to a variable with an ephemeral capability like iso^ should
  // produce a clear error message rather than crash the compiler.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let c: String iso^ = String";

  TEST_ERRORS_1(src, "Invalid type for field of assignment");
}

TEST_F(BadPonyTest, EphemeralParamWithDefaultArg)
{
  // From issue #4089
  const char* src =
    "class Foo\n"

    "actor Main\n"
    "  fun apply(x: Foo iso^ = Foo) => None\n"

    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src, "invalid parameter type for a parameter with a default argument");
}
  
TEST_F(BadPonyTest, MatchArrayPatternWithBareIntegerLiterals)
{
  // From issue #4554
  // Using bare integer literals in array match patterns used to crash
  // the compiler. Now it should produce a proper error message.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let arr: Array[U8 val] = [4; 5]\n"
    "    match arr\n"
    "    | [2; 3] => None\n"
    "    else\n"
    "      None\n"
    "    end";

  TEST_ERRORS_2(src,
    "couldn't find 'eq' in 'Array'",
    "this pattern element doesn't support structural equality");
}

TEST_F(BadPonyTest, MatchViewpointIsoCaptureWithoutConsume)
{
  // From issue #3596
  // Viewpoint-adapted iso capture from non-ephemeral field bypasses
  // the is_matchtype_with_consumed_pattern check.
  const char* src =
    "class B\n"
    "  var data: U64 = 99\n"

    "class Holder\n"
    "  let b: B iso = B\n"

    "  fun get(): this->B iso =>\n"
    "    match b\n"
    "    | let b': this->B iso => b'\n"
    "    end";

  TEST_ERRORS_1(src, "this capture is unsound");
}

TEST_F(BadPonyTest, MatchGenericCaptureWithoutConsume)
{
  // From issue #3596
  // Generic this->T capture where T could be iso bypasses
  // the is_matchtype_with_consumed_pattern check.
  const char* src =
    "class UsesNoConsume[T]\n"
    "  var value: (T | None) = None\n"

    "  fun ref set(t: T) =>\n"
    "    value = consume t\n"

    "  fun get(): this->T ? =>\n"
    "    match value\n"
    "    | let none: None => error\n"
    "    | let t: this->T => t\n"
    "    end";

  TEST_ERRORS_1(src, "this capture is unsound");
}

TEST_F(BadPonyTest, MatchViewpointIsoCaptureNoReturn)
{
  // From issue #3596
  // Even when the capture isn't returned, binding an iso from a
  // non-ephemeral field is unsound.
  const char* src =
    "class Holder2\n"
    "  let b: String iso = String\n"

    "  fun box bad() =>\n"
    "    match b\n"
    "    | let b': this->String iso => None\n"
    "    end";

  TEST_ERRORS_1(src, "this capture is unsound");
}

TEST_F(BadPonyTest, MatchIsoCaptureWithConsume)
{
  // From issue #3596
  // Consuming the match expression makes the discriminee ephemeral,
  // so iso captures are sound.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: String iso = recover iso String end\n"
    "    match consume x\n"
    "    | let y: String iso => None\n"
    "    end";

  // Soundness check is in the expr pass; stop before codegen.
  DO(test_compile(src, "expr"));
}

TEST_F(BadPonyTest, MatchNonIsoCaptureFromUnion)
{
  // From issue #3596
  // Non-iso captures (ref, val, box) from non-ephemeral discriminees are safe
  // and should not trigger the new soundness check.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: (String | U32) = \"hello\"\n"
    "    match x\n"
    "    | let s: String => None\n"
    "    end";

  // Soundness check is in the expr pass; stop before codegen.
  DO(test_compile(src, "expr"));
}

TEST_F(BadPonyTest, MatchViewpointRefCaptureFromField)
{
  // From issue #3596
  // Viewpoint-adapted captures with ref/val/box caps should not trigger
  // the new soundness check — only iso/trn/cap_any are dangerous.
  const char* src =
    "class Holder\n"
    "  let _s: (String | None) = \"hello\"\n"

    "  fun box get(): (this->String | None) =>\n"
    "    match _s\n"
    "    | let s: this->String => s\n"
    "    else\n"
    "      None\n"
    "    end\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  // Soundness check is in the expr pass; stop before codegen.
  DO(test_compile(src, "expr"));
}

TEST_F(BadPonyTest, MatchValConstraintCapture)
{
  // From issue #3596
  // Generic captures with val constraint don't need ephemeral since
  // val is safe to alias.
  const char* src =
    "class Container[K: Any val]\n"
    "  var _data: (K | None) = None\n"

    "  fun box lookup(): (this->K | None) =>\n"
    "    match _data\n"
    "    | let k: this->K => k\n"
    "    else\n"
    "      None\n"
    "    end\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  // Soundness check is in the expr pass; stop before codegen.
  DO(test_compile(src, "expr"));
}

TEST_F(BadPonyTest, MatchAliasedViewpointCapture)
{
  // From issue #3596
  // Already-aliased viewpoint captures (this->K!) should be accepted
  // since the aliased eph marker means aliasing won't change the capability.
  // This exercises the ast_id(eph) != TK_NONE early return in
  // capture_needs_ephemeral.
  const char* src =
    "class Container[K: Any #any]\n"
    "  var _data: (K | U32) = U32(0)\n"

    "  fun box lookup(): (this->K! | None) =>\n"
    "    match _data\n"
    "    | let k: this->K! => k\n"
    "    else\n"
    "      None\n"
    "    end\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  // Soundness check is in the expr pass; stop before codegen.
  DO(test_compile(src, "expr"));
}

TEST_F(BadPonyTest, MatchTupleViewpointIsoCaptureWithoutConsume)
{
  // Joe's review comment on PR #4975:
  // Viewpoint-adapted iso captures in tuple patterns must be checked
  // position by position. Both captures here need ephemeral but the
  // fields aren't consumed.
  const char* src =
    "class Foo\n"
    "  var data: U64 = 0\n"

    "class Holder\n"
    "  let a: Foo iso = Foo\n"
    "  let b: Foo iso = Foo\n"

    "  fun box bad() =>\n"
    "    match (a, b)\n"
    "    | (let x: this->Foo iso, let y: this->Foo iso) => None\n"
    "    end\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_2(src,
    "this capture is unsound",
    "this capture is unsound");
}

TEST_F(BadPonyTest, MatchUnionGenericIsoCaptureWithoutConsume)
{
  // Joe's review comment on PR #4975:
  // Generic iso capture from a non-ephemeral union is correctly rejected
  // by the new check_capture_soundness check.
  const char* src =
    "class Holder[T: Any #any]\n"
    "  var _a: (T | None) = None\n"

    "  fun box get(): (this->T | None) =>\n"
    "    match _a\n"
    "    | let t: this->T => t\n"
    "    else\n"
    "      None\n"
    "    end\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "this capture is unsound");
}

TEST_F(BadPonyTest, MatchGenericCaptureFromAliasedUnion)
{
  // Regression test for the interaction with PR #5145, which stopped
  // expanding type aliases during name resolution. The discriminee's
  // type is now seen as a TK_TYPEALIASREF rather than its expanded form,
  // and the soundness check must unfold the alias to find the ephemeral
  // member that makes the generic capture sound. This mirrors the failing
  // pattern in pony_check's Generator.value_iter.
  const char* src =
    "type AliasedResult[T2] is (T2^ | (T2^, U32))\n"

    "class Container[T: Any #any]\n"
    "  fun pick(): AliasedResult[T] ? => error\n"

    "  fun take() ? =>\n"
    "    match pick()?\n"
    "    | let v: T => None\n"
    "    end\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  // Soundness check is in the expr pass; stop before codegen.
  DO(test_compile(src, "expr"));
}

TEST_F(BadPonyTest, MatchAliasedCaptureTypeIsUnsound)
{
  // Regression test for the TK_TYPEALIASREF case in capture_needs_ephemeral.
  // When the capture's written type is itself a type alias whose underlying
  // form has a capability that changes under aliasing (iso/trn/#any/#send),
  // the check must unfold the alias to determine this. Without the unfold,
  // the soundness check silently accepts an unsound capture.
  const char* src =
    "type AnyT[T] is T\n"

    "class Holder[T: Any #any]\n"
    "  fun pick(): T ? => error\n"

    "  fun take() ? =>\n"
    "    match pick()?\n"
    "    | let v: AnyT[T] => None\n"
    "    end\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_1(src, "this capture is unsound");
}

TEST_F(BadPonyTest, MatchTuplePatternFromAliasedTupleUnsound)
{
  // Regression test for the interaction with PR #5145. When an alias
  // unfolds directly to a tuple type, the tuple-pattern walk in
  // check_capture_soundness must unfold the alias to see the underlying
  // TK_TUPLETYPE. Without the unfold, the tuple branch would silently
  // skip the check on alias-bearing tuple types — a false negative that
  // lets unsound generic captures through.
  const char* src =
    "type AliasedPair[T] is (T, T)\n"

    "class Container[T: Any #any]\n"
    "  fun pick(): AliasedPair[T] ? => error\n"

    "  fun take() ? =>\n"
    "    match pick()?\n"
    "    | (let a: T, let b: T) => None\n"
    "    end\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_2(src,
    "this capture is unsound",
    "this capture is unsound");
}

TEST_F(BadPonyTest, IfLiteralBranchWithTypecheckError)
{
  // Exercises the error path in expr_if where the first branch
  // produces a literal type (unparented TK_LITERAL accumulator)
  // and the second branch has a type error (#5214).
  const char* src =
    "primitive Foo\n"
    "  fun bar(x: U32): U32 => x\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    if true then 1 else Foo.bar(true) end";

  TEST_ERRORS_1(src, "argument not assignable to parameter");
}

TEST_F(BadPonyTest, IftypeLiteralBranchWithTypecheckError)
{
  // Same pattern for iftype (#5214).
  const char* src =
    "trait T\n"
    "class C is T\n"

    "primitive Foo\n"
    "  fun bar(x: U32): U32 => x\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    _test[C]()\n"

    "  fun _test[A: T]() =>\n"
    "    iftype A <: C then 1\n"
    "    else Foo.bar(true) end";

  TEST_ERRORS_1(src,
    "argument not assignable to parameter");
}

TEST_F(BadPonyTest, TryLiteralBodyWithTypecheckErrorElse)
{
  // Same pattern for try (#5214).
  const char* src =
    "primitive Foo\n"
    "  fun bar(x: U32): U32 => x\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try 1 else Foo.bar(true) end";

  TEST_ERRORS_1(src, "argument not assignable to parameter");
}

TEST_F(BadPonyTest, WhileLiteralBodyWithTypecheckErrorElse)
{
  // Same pattern for while: the body produces a literal type,
  // then the else clause has a type error (#5214).
  const char* src =
    "primitive Foo\n"
    "  fun bar(x: U32): U32 => x\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    while true do 1\n"
    "    else Foo.bar(true) end";

  TEST_ERRORS_1(src, "argument not assignable to parameter");
}

TEST_F(BadPonyTest, RepeatLiteralBodyWithTypecheckErrorElse)
{
  // Same pattern for repeat (#5214).
  const char* src =
    "primitive Foo\n"
    "  fun bar(x: U32): U32 => x\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    repeat 1 until true\n"
    "    else Foo.bar(true) end";

  TEST_ERRORS_1(src, "argument not assignable to parameter");
}

TEST_F(BadPonyTest, MatchLiteralCaseWithTypecheckErrorElse)
{
  // Same pattern for match: case body is a literal, else clause
  // has a type error. Non-exhaustive match so the else clause
  // is reached during type checking (#5214).
  const char* src =
    "primitive Foo\n"
    "  fun bar(x: U32): U32 => x\n"

    "class Bar\n"
    "class Baz\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let b: (Bar | Baz) = Bar\n"
    "    match b\n"
    "    | let _: Bar => 1\n"
    "    else\n"
    "      Foo.bar(true)\n"
    "    end";

  TEST_ERRORS_1(src, "argument not assignable to parameter");
}

TEST_F(BadPonyTest, RecursiveGenericInterfaceDoesNotHang)
{
  // Regression test for ponylang/ponyc#1216. A recursive generic
  // interface whose method return type references the same interface
  // with strictly larger type arguments hangs the structural subtype
  // check: each level has the same def pointer (Iter) but drifting
  // typeargs (Iter[A], Iter[(B, A)], Iter[(B, (B, A))], ...). The
  // structural cycle-detection in is_assumption_match
  // (src/libponyc/type/type_assume.c) requires exact structural
  // equality of typeargs and never matches drifting chains.
  //
  // The divergence guard in is_x_sub_x bounds the chain at
  // SAME_DEF_LIMIT same-def ancestors and bails with a diagnostic.
  //
  // The specific message matched below is incidental — the critical
  // property under test is that compilation terminates with a
  // diagnostic rather than hanging. The chrono budget is the
  // termination assertion; its 15-second cap is generous given the
  // post-fix run finishes in tens of milliseconds, with headroom for
  // Windows MSVC CI runners.
  //
  // Counterfactual reproduction (when the guard is reverted): the
  // program hangs indefinitely, so the ASSERT_LT below never fires —
  // the test is detected by running the binary under a shell-level
  // `timeout` and observing exit 124. See the commit introducing
  // the divergence guard in src/libponyc/type/subtype.c for the
  // counterfactual procedure.
  const char* src =
    "interface Iter[A]\n"
    "  fun enum[B](): Iter[(B, A)] => this\n"

    "actor Main\n"
    "  new create(env: Env) => None\n";

  auto start = std::chrono::steady_clock::now();
  TEST_ERRORS_1(src, "function body isn't the result type");
  auto elapsed = std::chrono::steady_clock::now() - start;

  auto seconds =
    std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
  ASSERT_LT(seconds, 15)
    << "Recursive generic interface compile took " << seconds
    << "s; expected <15s. Suggests the divergence guard in "
       "is_x_sub_x (src/libponyc/type/subtype.c) regressed. (Post-fix "
       "this finishes in tens of milliseconds; the 15s budget is "
       "headroom for Windows MSVC CI runners.)";
}

TEST_F(BadPonyTest, RecursiveGenericInterfaceEmitsGuardDiagnostic)
{
  // Companion to RecursiveGenericInterfaceDoesNotHang: when the
  // divergence guard in is_x_sub_x bails on a drifting recursion it
  // must write an ast_error_frame naming the guard, so a user who
  // trips it knows they hit a compiler safeguard rather than a real
  // subtype error. Without this diagnostic breadcrumb, bug reports
  // from guard-tripping stdlib authors would be incomprehensible —
  // they would see only a generic "not a subtype" error with no
  // indication it came from SAME_DEF_LIMIT.
  const char* src =
    "interface Iter[A]\n"
    "  fun enum[B](): Iter[(B, A)] => this\n"

    "actor Main\n"
    "  new create(env: Env) => None\n";

  const char* errs[] = {"function body isn't the result type", NULL};
  DO(test_expected_errors(src, "ir", errs));

  // Walk all error frames looking for the guard's diagnostic text.
  // The exact count and ordering of frames is incidental — what
  // matters is that somewhere in the error tree the guard's message
  // is visible. If the text below is refactored in subtype.c, update
  // this match too; the two are deliberately coupled.
  bool found_guard_frame = false;
  for(errormsg_t* e = errors_get_first(opt.check.errors);
    (e != NULL) && !found_guard_frame; e = e->next)
  {
    for(errormsg_t* ef = e->frame; ef != NULL; ef = ef->frame)
    {
      if(strstr(ef->msg, "recursion-divergence guard") != NULL)
      {
        found_guard_frame = true;
        break;
      }
    }
  }
  ASSERT_TRUE(found_guard_frame)
    << "Expected the divergence guard diagnostic frame to be present in "
    << "the error output, but it was not found. The guard in "
    << "is_x_sub_x (src/libponyc/type/subtype.c) must call "
    << "ast_error_frame when it bails so users know they hit "
    << "SAME_DEF_LIMIT rather than a real subtype error.";
}

TEST_F(BadPonyTest, RecursiveGenericInterfaceNestedWrappingDoesNotHang)
{
  // Companion regression for ponylang/ponyc#5198. The original issue
  // describes a hang on a recursive generic that drifts by wrapping
  // the interface around itself (I[A] -> I[I[A]]) rather than via
  // tuple typeargs as in #1216. Same divergence shape (drifting
  // same-def chain), same cause, same fix — the divergence guard in
  // is_x_sub_x bounds it.
  //
  // The specific error messages below are incidental — the critical
  // property under test is bounded-time failure with diagnostics.
  // Unlike the tupling-drift shape, the nested-wrapping shape also
  // fails the typearg-constraint check on the wrapped I[A], so two
  // errors are emitted.
  const char* src =
    "interface I[A]\n"
    "  fun f(): I[I[A]] => this\n"

    "actor Main\n"
    "  new create(env: Env) => None\n";

  auto start = std::chrono::steady_clock::now();
  TEST_ERRORS_2(src,
    "type argument is outside its constraint",
    "function body isn't the result type");
  auto elapsed = std::chrono::steady_clock::now() - start;

  auto seconds =
    std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
  ASSERT_LT(seconds, 15)
    << "Nested-wrapping recursive generic interface compile took "
    << seconds << "s; expected <15s. Suggests the divergence guard in "
       "is_x_sub_x (src/libponyc/type/subtype.c) regressed. (Post-fix "
       "this finishes in tens of milliseconds; the 15s budget is "
       "headroom for Windows MSVC CI runners.)";
}

TEST_F(BadPonyTest, RecursiveTypeParameterConstraintCompiles)
{
  // Regression test for ponylang/ponyc#3930. A type parameter whose
  // constraint references the parameter itself
  // (`A: Array[Array[A]]`) previously caused a stack overflow in the
  // subtype checker's semantic typearg equality, crashing the
  // compiler.
  //
  // This test lives alongside the recursive-interface regression
  // tests because the underlying fix is shared: both symptoms come
  // from typearg equality re-entering the subtype checker, which
  // eventually re-enters the coinductive assume machinery
  // (type_assume) for the same pair — unbounded at depth for
  // recursive type parameter constraints, and exponentially at each
  // guard-capped chain for nested-wrapping interfaces.
  //
  // Unlike the other recursive-shape tests in this file, this source
  // is well-formed and must compile successfully.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) => None\n"

    "  fun flatten[A: Array[Array[A]] #read](arrayin: Array[Array[A]])\n"
    "    : Array[A]\n"
    "  =>\n"
    "    let rv: Array[A] = Array[A]\n"
    "    for f in arrayin.values() do\n"
    "      for g in f.values() do\n"
    "        rv.push(g)\n"
    "      end\n"
    "    end\n"
    "    rv";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, FBoundedInterfaceWithIntersectionCompiles)
{
  // Project-level regression guard for ponylang/ponyc#2399. The
  // original issue report describes a segfault on this program, but
  // the program already compiles cleanly on the parent of this PR's
  // first commit — some earlier change incidentally fixed it. This
  // test is kept as a guard against the shape regressing in future
  // work, not as proof that this PR is what fixes #2399.
  //
  // The program is well-formed and must compile successfully.
  const char* src =
    "interface val X[Y: X[Y]]\n"
    "  fun apply(y: X[Y])\n"

    "type C is ((A | B) & X[(A | B)])\n"

    "primitive A is X[C]\n"
    "  fun apply(y: X[C]) => None\n"

    "primitive B is X[C]\n"
    "  fun apply(y: X[C]) => None\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, FBoundedTraitWithSelfApplyEmitsConstraintError)
{
  // Project-level regression guard for ponylang/ponyc#2562. The
  // original issue report describes a segfault on this program, but
  // the program already produces clean type errors on the parent of
  // this PR's first commit — some earlier change incidentally fixed
  // it. This test is kept as a guard against the shape regressing in
  // future work, not as proof that this PR is what fixes #2562.
  //
  // The program is ill-formed: the `apply[X[A]](this)` call produces
  // a cap mismatch between the inferred argument type and the method
  // type parameter's constraint. The critical property under test is
  // bounded-time failure with a diagnostic — the exact wording is
  // incidental.
  const char* src =
    "trait X[A: X[A] box]\n"
    "  fun apply[B: X[B] box](a: B) =>\n"
    "    None\n"
    "  fun f() =>\n"
    "    apply[X[A]](this)\n"

    "class C is X[C box]\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERRORS_2(src,
    "type argument is outside its constraint",
    "type argument is outside its constraint");
}

TEST_F(BadPonyTest, SelfReferentialConstraintInUnionErrors)
{
  // Regression test for ponylang/ponyc#2497. A type parameter that
  // references itself as a member of a union in its own constraint used
  // to crash the compiler with a stack overflow: the subtype checker
  // replaces the parameter with its constraint and re-decomposes the
  // union back into the parameter forever (is_typeparam_sub_x in
  // src/libponyc/type/subtype.c, which has no cycle guard for
  // typeparamref pairs). It must now be a clean compile error.
  const char* src =
    "class C\n"
    "class A[B: (B | C)]\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERROR_WITH_NOTE(src,
    "type parameter 'B' can't appear directly in its own constraint",
    "constraint is here");
}

TEST_F(BadPonyTest, SelfReferentialConstraintInUnionReversedErrors)
{
  // The same illegal shape as SelfReferentialConstraintInUnionErrors with
  // the union members reversed. This order happened to compile before the
  // fix (the crash was evaluation-order dependent), so it guards against
  // the order-sensitive half-fix.
  const char* src =
    "class C\n"
    "class A[B: (C | B)]\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERROR_WITH_NOTE(src,
    "type parameter 'B' can't appear directly in its own constraint",
    "constraint is here");
}

TEST_F(BadPonyTest, SelfReferentialConstraintInIntersectionErrors)
{
  // The self-reference sits in an intersection member rather than a union
  // member. The intersection is itself a union member so the constraint
  // resolves a capability (otherwise the "no valid capability" check fires
  // first); this exercises the intersection arm of the cycle walk.
  const char* src =
    "trait T\n"
    "class A[B: (None | (B & T))]\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERROR_WITH_NOTE(src,
    "type parameter 'B' can't appear directly in its own constraint",
    "constraint is here");
}

TEST_F(BadPonyTest, MutuallyRecursiveConstraintInUnionErrors)
{
  // Mutual recursion through compound constraints (Praetonus's example in
  // ponylang/ponyc#2497): A's constraint names B, B's names A. Each is
  // self-referential as a cycle, so it must be rejected. Bare mutual
  // chains (`[A: B, B: A]`) collapse to "unconstrained" and stay legal;
  // this case does not, because the references are union members.
  const char* src =
    "class C\n"
    "  fun foo[A: (B | None), B: (A | None)]() =>\n"
    "    None\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  // Both A and B sit in the cycle, so each is flagged at its own definition.
  TEST_ERRORS_2(src,
    "type parameter 'A' can't appear directly in its own constraint",
    "type parameter 'B' can't appear directly in its own constraint");
}

TEST_F(BadPonyTest, TransitiveSelfReferentialConstraintErrors)
{
  // The self-reference is reached transitively: X is constrained by Y,
  // and Y's constraint names X as a union member. typeparam_constraint
  // resolves X's effective constraint to Y's `(X | None)`, which contains
  // X, so the cycle must be rejected.
  const char* src =
    "class A[X: Y, Y: (X | None)]\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERROR_WITH_NOTE(src,
    "can't appear directly in its own constraint",
    "constraint is here");
}

TEST_F(BadPonyTest, NestedSelfReferentialConstraintInUnionErrors)
{
  // The self-reference is nested one level deeper than
  // SelfReferentialConstraintInUnionErrors. Reaching it depends on the
  // constraint-tuple scan in the flatten pass terminating on nested
  // unions (it previously recursed forever on the inner union, crashing
  // before this check could run). It must produce the same clean error.
  const char* src =
    "class C\n"
    "class A[B: (B | (C | B))]\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERROR_WITH_NOTE(src,
    "type parameter 'B' can't appear directly in its own constraint",
    "constraint is here");
}

TEST_F(BadPonyTest, SelfReferentialConstraintViaParameterizedAliasErrors)
{
  // The self-reference is revealed by unfolding a parameterized type
  // alias: `MyU[B]` expands to `(B | None)`, so B appears as a bare union
  // member of its own constraint. This exercises the alias-unfold arm of
  // the cycle walk and must be rejected.
  const char* src =
    "type MyU[X] is (X | None)\n"
    "class A[B: MyU[B]]\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_ERROR_WITH_NOTE(src,
    "type parameter 'B' can't appear directly in its own constraint",
    "constraint is here");
}

TEST_F(BadPonyTest, SelfReferenceBehindNominalConstraintCompiles)
{
  // The carve-out for ponylang/ponyc#2497: a type parameter may reference
  // itself when the reference is behind a constructor (a nominal typearg).
  // Here B appears only inside `Array[B]`, a union member, so the subtype
  // checker terminates via its nominal cycle guard. This is well-formed
  // and must compile.
  const char* src =
    "class A[B: (None | Array[B])]\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, UnconstrainedTypeParameterCompiles)
{
  // Guard against the #2497 fix over-reaching. An unconstrained type
  // parameter `[A]` is sugared to `A: A` and resolves to a self-
  // referential typeparamref, which typeparam_constraint collapses to
  // "no constraint". This must not be mistaken for an illegal self-
  // referential constraint.
  const char* src =
    "class A[B]\n"

    "actor Main\n"
    "  new create(env: Env) => None";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, WhileBodyAndElseJumpAwayInSeparateFunction)
{
  // From issue #2792 (first example). A while loop whose body and else
  // both jump away (break + return) used to trigger a typecheck assertion
  // when the loop appeared in a separate function rather than directly in
  // create. expr_seq tried to type the jumps-away while and reported a
  // typecheck error without registering an actual error message,
  // tripping the "errors must be > 0" assertion in pass_expr.
  const char* src =
    "actor Main\n"
    "  fun a() =>\n"
    "    while true do\n"
    "      break\n"
    "    else\n"
    "      return\n"
    "    end\n"

    "  new create(env: Env) =>\n"
    "    a()";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, RepeatBodyAndElseJumpAwayInSeparateFunction)
{
  // Same shape as WhileBodyAndElseJumpAwayInSeparateFunction but for
  // repeat. The fix in expr_seq is loop-agnostic; this guards against a
  // future regression that special-cases one loop kind.
  const char* src =
    "actor Main\n"
    "  fun a() =>\n"
    "    repeat\n"
    "      break\n"
    "    until true\n"
    "    else\n"
    "      return\n"
    "    end\n"

    "  new create(env: Env) =>\n"
    "    a()";

  TEST_COMPILE(src);
}

TEST_F(BadPonyTest, WhileWithLiteralBreakValueAndJumpsAwayElse)
{
  // Regression test for the literal-break-value variant of #2792. A while
  // whose body breaks with a literal value and whose else jumps away
  // gives the loop a literal type. The fix for #2792 must not skip the
  // unused-literal check on jumps-away children, otherwise the literal
  // type propagates to codegen and trips a reach.c assertion.
  const char* src =
    "actor Main\n"
    "  fun a(): U32 =>\n"
    "    while true do\n"
    "      break 5\n"
    "    else\n"
    "      return 6\n"
    "    end\n"
    "    7\n"

    "  new create(env: Env) =>\n"
    "    a()";

  TEST_ERRORS_1(src, "Cannot infer type of unused literal");
}

TEST_F(BadPonyTest, RepeatWithLiteralBreakValueAndJumpsAwayElse)
{
  // Same shape as WhileWithLiteralBreakValueAndJumpsAwayElse but for
  // repeat.
  const char* src =
    "actor Main\n"
    "  fun a(): U32 =>\n"
    "    repeat\n"
    "      break 5\n"
    "    until true\n"
    "    else\n"
    "      return 6\n"
    "    end\n"
    "    7\n"

    "  new create(env: Env) =>\n"
    "    a()";

  TEST_ERRORS_1(src, "Cannot infer type of unused literal");
}

// A control expression that jumps away with no value (`error`, `return`,
// `break`, `continue`) has no type. Used in a value-operand position it must
// produce a clean error, not crash the compiler. These were all assertion
// crashes in the expr pass; uncovered while investigating issue #3326.

TEST_F(BadPonyTest, IfConditionJumpsAway)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try let x: U8 = if error then U8(1) else U8(2) end end";

  TEST_ERRORS_1(src,
    "a condition can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, WhileConditionJumpsAway)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try let x: U8 = while error do U8(1) else U8(2) end end";

  TEST_ERRORS_1(src,
    "a condition can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, RepeatConditionJumpsAway)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try let x: U8 = repeat U8(1) until error else U8(2) end end";

  TEST_ERRORS_1(src,
    "a condition can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, RecoverOperandJumpsAway)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try let x: U8 = recover error end end";

  TEST_ERRORS_1(src,
    "a recover operand can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, MatchOperandJumpsAway)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try let x: U8 = match error | let y: U8 => y else U8(0) end end";

  TEST_ERRORS_1(src,
    "a match operand can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, MatchOperandJumpsAwayMultipleCases)
{
  // The operand is checked per-case and once at the match; with multiple cases
  // the error must be reported exactly once (TEST_ERRORS_1 enforces the count).
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try\n"
    "      let x: U8 = match error\n"
    "      | let y: U8 => y\n"
    "      | let z: U16 => U8(0)\n"
    "      else U8(0) end\n"
    "    end";

  TEST_ERRORS_1(src,
    "a match operand can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, MatchGuardJumpsAway)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try let x: U8 = match U8(1) | let y: U8 if error => y else U8(0) end "
    "end";

  TEST_ERRORS_1(src,
    "a match guard can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, FunctionArgumentJumpsAway)
{
  const char* src =
    "actor Main\n"
    "  fun f(n: U8): U8 => n\n"
    "  new create(env: Env) =>\n"
    "    try let x: U8 = f(error) end";

  TEST_ERRORS_1(src,
    "an argument can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, ArgumentReturnsAway)
{
  // The guard keys on AST_FLAG_JUMPS_AWAY, which `return` sets just like
  // `error`; confirm a non-error jump is rejected the same way.
  const char* src =
    "actor Main\n"
    "  fun f(n: U8): U8 => n\n"
    "  new create(env: Env) =>\n"
    "    let x: U8 = f(return)";

  TEST_ERRORS_1(src,
    "an argument can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, ConstructorArgumentJumpsAway)
{
  const char* src =
    "class C\n"
    "  new create(n: U8) => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try let x: C = C(error) end";

  TEST_ERRORS_1(src,
    "an argument can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, MethodReceiverJumpsAway)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try let x: String = (error).string() end";

  TEST_ERRORS_1(src,
    "a receiver can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, QualifiedReceiverJumpsAway)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try let x = (error)[U8] end";

  TEST_ERRORS_1(src,
    "a receiver can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, AsOperandJumpsAway)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try let x = (error) as U8 end";

  TEST_ERRORS_1(src,
    "a cast operand can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, IsOperandJumpsAway)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try let x: Bool = (error) is U8(1) end";

  TEST_ERRORS_1(src,
    "an operand of an identity comparison can't be an expression that jumps "
    "away with no value");
}

TEST_F(BadPonyTest, IsntRightOperandJumpsAway)
{
  // Jump-away on the right operand, exercising the second `||` clause that the
  // left-operand tests above can't reach.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try let x: Bool = U8(1) isnt (error) end";

  TEST_ERRORS_1(src,
    "an operand of an identity comparison can't be an expression that jumps "
    "away with no value");
}

TEST_F(BadPonyTest, FFIArgumentJumpsAway)
{
  const char* src =
    "use @exit[None](code: I32)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try @exit(error) end";

  TEST_ERRORS_1(src,
    "an argument can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, FFIVariadicArgumentJumpsAway)
{
  // Exercises the second FFI argument loop (variadic args past the declared
  // parameters), a separate guard from the declared-parameter loop above.
  const char* src =
    "use @f[None](a: U8, ...)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try @f(U8(1), error) end";

  TEST_ERRORS_1(src,
    "an argument can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, CallReceiverJumpsAway)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try let x = (error)(U8(1)) end";

  TEST_ERRORS_1(src,
    "a receiver can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, DefaultArgumentJumpsAway)
{
  const char* src =
    "actor Main\n"
    "  fun f(a: U8 = (error)): U8 => a\n"
    "  new create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a default argument can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, LambdaCaptureJumpsAway)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let l = {()(x = (error)) => x}";

  TEST_ERRORS_1(src,
    "a lambda capture can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, LambdaDontcareCaptureJumpsAway)
{
  // A typed capture into `_` takes a different branch (a subtype check) than
  // the untyped capture above; both must reject a jump-away value.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let l = {()(_: U8 = (error)) => None}";

  TEST_ERRORS_1(src,
    "a lambda capture can't be an expression that jumps away with no value");
}

TEST_F(BadPonyTest, TupleElementJumpsAwayAfterLiteral)
{
  // The jumps-away element follows a literal element, which used to take a
  // short-circuit that skipped the later element's check.
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try let t: (U8, U8) = (1, (error)) end";

  TEST_ERRORS_1(src,
    "a tuple can't contain an expression that jumps away with no value");
}

TEST_F(BadPonyTest, TuplePatternElementJumpsAwayAfterLiteral)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    try match (U8(1), U8(2)) | (1, (error)) => None end end";

  TEST_ERRORS_1(src,
    "a tuple can't contain an expression that jumps away with no value");
}
