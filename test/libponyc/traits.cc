#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_EQUIV(src, expect) DO(test_equiv(src, "expr", expect, "expr"))


class TraitsTest : public PassTest
{};


// Basics

TEST_F(TraitsTest, InheritMethod)
{
  const char* src =
    "trait T1\n"
    "  fun f(): U32\n"

    "trait T2 is T1";

  const char* expect =
    "trait T1\n"
    "  fun f(): U32\n"

    "trait T2 is T1\n"
    "  fun f(): U32";

  TEST_EQUIV(src, expect);
}


TEST_F(TraitsTest, TransitiveTraits)
{
  const char* src =
    "trait T1\n"
    "  fun f1(): U32\n"

    "trait T2 is T1\n"
    "  fun f2(): Bool\n"

    "trait T3 is T2";

  const char* expect =
    "trait T1\n"
    "  fun f1(): U32\n"

    "trait T2 is T1\n"
    "  fun f2(): Bool\n"
    "  fun f1(): U32\n"

    "trait T3 is T2\n"
    "  fun f2(): Bool\n"
    "  fun f1(): U32";

  TEST_EQUIV(src, expect);
}


TEST_F(TraitsTest, CannotProvideClass)
{
  const char* src =
    "class C1\n"
    "class C2 is C1";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, SingleTraitLoop)
{
  const char* src =
    "trait T is T\n"
    "class C is T";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, MultiTraitLoop)
{
  const char* src =
    "trait T1 is T2\n"
    "trait T2 is T3\n"
    "trait T3 is T1";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, TraitAndClassNamesDontClash)
{
  const char* src =
    "trait T\n"
    "  fun f(y: U32): U32 => y\n"

    "class C is T\n"
    "  var y: Bool = false";

  TEST_COMPILE(src);
}


TEST_F(TraitsTest, TraitMethodAndCLassFieldNamesDoClash)
{
  const char* src =
    "trait T\n"
    "  fun f()\n"

    "class C is T\n"
    "  var f: Bool = false";

  TEST_ERROR(src);
}


// Multiple inheritance without a local definition

TEST_F(TraitsTest, MultiInheritNoLocal)
{
  const char* src =
    "trait A\n"
    "trait B\n"

    "trait T1\n"
    "  fun f(x: A, y: U32 = 1): B\n"

    "trait T2\n"
    "  fun f(x: A, y: U32 = 1): B\n"

    "trait T3 is (T1 & T2)";

  const char* expect =
    "trait A\n"
    "trait B\n"

    "trait T1\n"
    "  fun f(x: A, y: U32 = 1): B\n"

    "trait T2\n"
    "  fun f(x: A, y: U32 = 1): B\n"

    "trait T3 is (T1 & T2)\n"
    "  fun f(x: A, y: U32 = 1): B";

    TEST_EQUIV(src, expect);
}


TEST_F(TraitsTest, MultiInheritNoLocalParamTypeFail)
{
  const char* src =
    "trait A\n"
    "trait B\n"

    "trait T1\n"
    "  fun f(x: A)\n"

    "trait T2\n"
    "  fun f(x: B)\n"

    "trait T3 is T1, T2";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, MultiInheritNoLocalParamNameFail)
{
  const char* src =
    "trait A\n"

    "trait T1\n"
    "  fun f(x: A)\n"

    "trait T2\n"
    "  fun f(y: A)\n"

    "trait T3 is T1, T2";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, MultiInheritNoLocalParamDefaultValueFail)
{
  const char* src =
    "trait T1\n"
    "  fun f(x: U32 = 1)\n"

    "trait T2\n"
    "  fun f(x: U32 = 2)\n"

    "trait T3 is T1, T2";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, MultiInheritNoLocalReturnTypeFail)
{
  const char* src =
    "trait A\n"
    "trait B\n"

    "trait T1\n"
    "  fun f(): A\n"

    "trait T2\n"
    "  fun f(): B\n"

    "trait T3 is T1, T2";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, MultiInheritNoLocalErrorsFail)
{
  const char* src =
    "trait T1\n"
    "  fun f() ?\n"

    "trait T2\n"
    "  fun f()\n"

    "trait T3 is T1, T2";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, MultiInheritNoLocalCapFail)
{
  const char* src =
    "trait T1\n"
    "  fun box f()\n"

    "trait T2\n"
    "  fun ref f()\n"

    "trait T3 is T1, T2";

  TEST_ERROR(src);
}



// Contra- and co-variance

TEST_F(TraitsTest, ParamWrongTypeFail)
{
  const char* src =
    "trait A\n"
    "trait B\n"

    "trait T1\n"
    "  fun f(x: A)\n"

    "trait T2 is T1\n"
    "  fun f(x: B)";

  TEST_ERROR(src);
}

TEST_F(TraitsTest, ReturnWrongTypeFail)
{
  const char* src =
    "trait A\n"
    "trait B\n"

    "trait T1\n"
    "  fun f(): A\n"

    "trait T2 is T1\n"
    "  fun f(): B";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, ParamContravariance)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(x: A)\n"

    "trait T2 is T1\n"
    "  fun f(x: AB)";

  TEST_COMPILE(src);
}


TEST_F(TraitsTest, ParamCovarianceFail)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(x: AB)\n"

    "trait T2 is T1\n"
    "  fun f(x: A)";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, MultiParamContravariance)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(x: A)\n"

    "trait T2\n"
    "  fun f(x: AB)\n"

    "trait T3 is (T1 & T2)\n"
    "  fun f(x: AB)";

  TEST_COMPILE(src);
}


TEST_F(TraitsTest, MultiParamCovarianceFail)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(x: A)\n"

    "trait T2\n"
    "  fun f(x: AB)\n"

    "trait T3 is (T1 & T2)\n"
    "  fun f(x: A)";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, IndependentParamContravariance)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(x: A)\n"

    "trait T2\n"
    "  fun f(x: B)\n"

    "trait T3 is (T1 & T2)\n"
    "  fun f(x: AB)";

  TEST_COMPILE(src);
}


TEST_F(TraitsTest, ReturnCovariance)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(): AB\n"

    "trait T2 is T1\n"
    "  fun f(): A";

  TEST_COMPILE(src);
}


TEST_F(TraitsTest, ReturnContravarianceFail)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(): A\n"

    "trait T2 is T1\n"
    "  fun f(): AB";

  TEST_ERROR(src);
}


// Default bodies

TEST_F(TraitsTest, ClassGetsTraitBody)
{
  // Need to explicitly state C.create() as otherwise it's added AFTER the
  // functions from T and our ASTs won't match

  const char* src =
    "trait T\n"
    "  fun f(): U32 => 1\n"
    "class C is T\n"
    "  new create() => true";

  const char* expect =
    "trait T\n"
    "  fun f(): U32 => 1\n"
    "class C is T\n"
    "  new create() => true\n"
    "  fun f(): U32 => 1";

  TEST_EQUIV(src, expect);
}


TEST_F(TraitsTest, ClassBodyNotOverriddenByTrait)
{
  const char* src =
    "trait T\n"
    "  fun f(): U32 => 1\n"
    "class C is T\n"
    "  fun f(): U32 => 2";

  TEST_COMPILE(src);

  ast_t* f = lookup_member("C", "f");
  ASSERT_ID(TK_FUN, f);

  lexint_t* value = ast_int(ast_child(ast_childidx(f, 7)));

  ASSERT_EQ(0, value->high);
  ASSERT_EQ(2, value->low);
}


TEST_F(TraitsTest, NoBodyFail)
{
  const char* src =
    "trait T\n"
    "  fun f(): U32\n"
    "class C is T";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, AmbiguousBodyTrait)
{
  const char* src =
    "trait T1\n"
    "  fun f(): U32 => 1\n"

    "trait T2\n"
    "  fun f(): U32 => 2\n"

    "trait T3 is (T1 & T2)";

  const char* expect =
    "trait T1\n"
    "  fun f(): U32 => 1\n"

    "trait T2\n"
    "  fun f(): U32 => 2\n"

    "trait T3 is (T1 & T2)\n"
    "  fun f(): U32\n";

  TEST_EQUIV(src, expect);
}


TEST_F(TraitsTest, AmbiguousBodyClassFail)
{
  const char* src =
    "trait T1\n"
    "  fun f(): U32 => 1\n"
    "trait T2\n"
    "  fun f(): U32 => 2\n"
    "class C is (T1 & T2)";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, AmbiguousBodyPropogates)
{
  const char* src =
    "trait T1\n"
    "  fun f(): U32 => 1\n"
    "trait T2\n"
    "  fun f(): U32 => 2\n"
    "trait T3\n"
    "  fun f(): U32 => 3\n"

    "trait T12 is (T1 & T2)\n"

    "class C is (T12 & T3)";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, ParentOverridesGrandparentBody)
{
  const char* src =
    "trait T1\n"
    "  fun f(): U32 => 1\n"

    "trait T2 is T1\n"
    "  fun f(): U32 => 2\n"

    "class C is T2\n"
    "  new create() => true";

  const char* expect =
    "trait T1\n"
    "  fun f(): U32 => 1\n"

    "trait T2 is T1\n"
    "  fun f(): U32 => 2\n"

    "class C is T2\n"
    "  new create() => true\n"
    "  fun f(): U32 => 2";

  TEST_EQUIV(src, expect);
}


TEST_F(TraitsTest, DiamondInheritance)
{
  const char* src =
    "trait T1\n"
    "  fun f(): U32 => 1\n"

    "trait T2 is T1\n"
    "trait T3 is T1\n"

    "class C is (T2 & T3)\n"
    "  new create() => true";

  const char* expect =
    "trait T1\n"
    "  fun f(): U32 => 1\n"

    "trait T2 is T1\n"
    "  fun f(): U32 => 1\n"

    "trait T3 is T1\n"
    "  fun f(): U32 => 1\n"

    "class C is (T2 & T3)\n"
    "  new create() => true\n"
    "  fun f(): U32 => 1";

  TEST_EQUIV(src, expect);
}

TEST_F(TraitsTest, DefaultUsedInIntersection)
{
  const char* src =
    "trait T1\n"
    "  fun f(): U32\n"

    "trait T2\n"
    "  fun f(): U32 =>\n"
    "    let x: U32 = 1\n"
    "    x\n"

    "class C is (T1 & T2)\n";

  TEST_COMPILE(src);
}


// Method variety

TEST_F(TraitsTest, ClassInheritFunction)
{
  const char* src =
    "trait T\n"
    "  fun f() => None\n"

    "class C is T";

  TEST_COMPILE(src);
}


TEST_F(TraitsTest, ClassInheritBehaviourFail)
{
  const char* src =
    "trait T\n"
    "  be f() => None\n"

    "class C is T";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, PrimitiveInheritFunction)
{
  const char* src =
    "trait T\n"
    "  fun f() => None\n"

    "primitive P is T";

  TEST_COMPILE(src);
}


TEST_F(TraitsTest, PrimitiveInheritBehaviourFail)
{
  const char* src =
    "trait T\n"
    "  be f() => None\n"

    "primitive P is T";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, ActorInheritFunction)
{
  const char* src =
    "trait T\n"
    "  fun f() => None\n"

    "actor A is T";

  TEST_COMPILE(src);
}


TEST_F(TraitsTest, ActorInheritBehaviour)
{
  const char* src =
    "trait T\n"
    "  be f() => None\n"

    "actor A is T";

  TEST_COMPILE(src);
}


TEST_F(TraitsTest, LetInFunction)
{
  const char* src =
    "trait T\n"
    " fun foo() =>\n"
    "   let x: U32 = 0\n"

    "class Baz is T\n"
    "class Bar is T\n";

  // Tests #684. Issue only kicks in expr pass when
  // let single assignment is checked, but test seems
  // to belong here as fix is in traits.c.
  DO(test_compile(src, "expr"));
}


TEST_F(TraitsTest, TypeargInFunction)
{
  const char* src =
    "trait T[A]\n"
    " fun foo(x: A): A =>\n"
    "   consume x\n"

    "class Baz is T[U32]\n"
    " fun bar(): U32 => foo(U32(0))\n";

  // Tests typearg reification in methods inherited from parameterised traits.
  DO(test_compile(src, "expr"));
}


TEST_F(TraitsTest, TypeargInFunction2)
{
  const char* src =
    "trait T1\n"
    " new val create()\n"

    "trait T2\n"

    "primitive P is (T1 & T2)\n"

    "trait T3[A: T1 val]\n"
    " fun foo(): A =>\n"
    "   A\n"

    "class Baz is T3[P]\n"
    " fun bar(): P => foo()\n";

  // Tests typearg reification in methods inherited from parameterised traits.
  DO(test_compile(src, "expr"));
}


TEST_F(TraitsTest, TypeParamRefBeforeTypeParam)
{
  const char* src =
    "trait T\n"
    "  fun f[X: Y, Y: U32]() => None\n"

    "class C is T";

  TEST_COMPILE(src);
}


TEST_F(TraitsTest, InterfaceStructuralMissingSpecializations)
{
  const char* src =
    "interface Describable[A: Any val]\n"
    "  fun describe(): None\n"
    "  fun describe(): None iftype A <: Stringable val\n"

    "class Container[A: Any val]\n"
    "  fun describe(): None => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let c: Describable[U32] = Container[U32]";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, LocalMethodGroupAcceptsEquivalentInheritedGuard)
{
  const char* src =
    "trait Group[A: Any val]\n"
    "  fun check(): U64 => 10\n"
    "  fun check(): U64 iftype A <: First => 20\n"

    "primitive Combined[A: Any val] is Group[A]\n"
    "  fun check(): U64 => 30\n"
    "  fun check(): U64 iftype A <: Second => 40\n"

    "interface val First\n"
    "  fun value(): U8\n"

    "interface val Second\n"
    "  fun value(): U8";

  DO(test_compile(src, "verify"));
}


// Member shadowing

TEST_F(TraitsTest, ParamShadowsOwnMethod)
{
  const char* src =
    "actor A\n"
    "  fun foo(foo: U32) => None";

  TEST_ERROR(src);
}

TEST_F(TraitsTest, ParamShadowsOwnField)
{
  const char* src =
    "class A\n"
    "  let x: U32 = 0\n"
    "  fun foo(x: U32) => None";

  TEST_ERROR(src);
}

TEST_F(TraitsTest, ParamShadowsInheritedMethod)
{
  const char* src =
    "interface Foo\n"
    "  fun apply(): USize => 42\n"

    "class Bar is Foo\n"
    "  new create(apply: {(): USize}) => None";

  TEST_ERROR(src);
}

TEST_F(TraitsTest, LocalShadowsInheritedMethod)
{
  const char* src =
    "interface Foo\n"
    "  fun apply(): USize => 42\n"

    "class Bar is Foo\n"
    "  new create() =>\n"
    "    let apply: USize = 10";

  TEST_ERROR(src);
}

TEST_F(TraitsTest, InheritedBodyLocalShadowsMethod)
{
  const char* src =
    "interface ShadowWarrior\n"
    "  fun boo(): String =>\n"
    "    let name: String = \"abc\"\n"
    "    name\n"

    "class X is ShadowWarrior\n"
    "  fun name(): String => \"X\"";

  TEST_ERROR(src);
}

TEST_F(TraitsTest, NoShadowInLambda)
{
  const char* src =
    "interface Foo\n"
    "  fun name(): String => \"Foo\"\n"

    "class Bar is Foo\n"
    "  new create() =>\n"
    "    let f = {(name: String): String => name}";

  TEST_COMPILE(src);
}

TEST_F(TraitsTest, InheritedMethodNoConflict)
{
  const char* src =
    "interface Foo\n"
    "  fun apply(): USize => 42\n"

    "class Bar is Foo\n"
    "  new create() => None";

  TEST_COMPILE(src);
}

TEST_F(TraitsTest, CaseInsensitiveClashWithInheritedMethod)
{
  const char* src =
    "interface Foo\n"
    "  fun apply(): USize => 42\n"

    "class Bar is Foo\n"
    "  new create() =>\n"
    "    let Apply: USize = 10";

  TEST_ERROR(src);
}

TEST_F(TraitsTest, MatchCaptureShadowsInheritedMethod)
{
  const char* src =
    "interface Foo\n"
    "  fun apply(): USize => 42\n"

    "class Bar is Foo\n"
    "  new create() =>\n"
    "    match USize(10)\n"
    "    | let apply: USize => None\n"
    "    end";

  TEST_ERROR(src);
}

TEST_F(TraitsTest, ParamShadowsInheritedBehaviour)
{
  const char* src =
    "trait Foo\n"
    "  be run()\n"

    "actor Bar is Foo\n"
    "  be run() => None\n"
    "  fun foo(run: U32) => None";

  TEST_ERROR(src);
}

TEST_F(TraitsTest, ParamShadowsInheritedMethodGroup)
{
  const char* src =
    "class Foo[A: Any val]\n"
    "  fun bar(): None => None\n"
    "  fun bar(): None iftype A <: Stringable val => None\n"
    "  fun baz(bar: U32) => None";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, MultiInheritGenericMethodClassTypeParamSameName)
{
  const char* src =
    "trait T1\n"
    "  fun f[T: Any val](x: T): T\n"

    "trait T2\n"
    "  fun f[T: Any val](x: T): T => x\n"

    "class C[T: Any val] is (T1 & T2)";

  TEST_COMPILE(src);
}

TEST_F(TraitsTest, MultiInheritGenericMethodTypeParamNameFail)
{
  const char* src =
    "trait T1\n"
    "  fun f[T: Any val](x: T): T\n"

    "trait T2\n"
    "  fun f[U: Any val](x: U): U\n"

    "class C is (T1 & T2)";

  const char* errs[] = {"clashing definitions for method 'f'", NULL};
  DO(test_expected_errors(src, "expr", errs));
}

TEST_F(TraitsTest, MultiInheritGenericClassVsMethodTypeParamFail)
{
  // In T1's copy, x has the class's T. In T2's copy, x has the method's own T.
  const char* src =
    "trait T1[X: Any val]\n"
    "  fun f[T: Any val](x: X): X\n"

    "trait T2\n"
    "  fun f[T: Any val](x: T): T => x\n"

    "class C[T: Any val] is (T1[T] & T2)";

  const char* errs[] = {"clashing definitions for method 'f'", NULL};
  DO(test_expected_errors(src, "expr", errs));
}

TEST_F(TraitsTest, MultiInheritGenericMethodDifferentTypeParamPositionFail)
{
  // The parameter has the method's first type parameter in T1 and its second
  // in T2.
  const char* src =
    "trait T1\n"
    "  fun f[A: Any val, B: Any val](x: A): A\n"

    "trait T2\n"
    "  fun f[A: Any val, B: Any val](x: B): B\n"

    "class C is (T1 & T2)";

  const char* errs[] = {"clashing definitions for method 'f'", NULL};
  DO(test_expected_errors(src, "expr", errs));
}

TEST_F(TraitsTest, MultiInheritGenericObjectLiteralVsMethodTypeParamFail)
{
  // The object literal's R is copied from make's R. It is the object
  // literal's own type parameter, not a method's, so A's copy of g, whose
  // parameter has the object literal's R, clashes with B's copy, whose
  // parameter has g's own Q.
  const char* src =
    "trait A[T: Any val]\n"
    "  fun g[Q: Any val](x: T): None\n"

    "trait B\n"
    "  fun g[Q: Any val](x: Q): None => None\n"

    "primitive Maker\n"
    "  fun make[R: Any val](): A[R] =>\n"
    "    object is (A[R] & B) end";

  const char* errs[] = {"clashing definitions for method 'g'", NULL};
  DO(test_expected_errors(src, "expr", errs));
}

TEST_F(TraitsTest, MultiInheritGenericMethodTypeParamEphemeralFail)
{
  // Both parameters have the method's first type parameter, but only T1's is
  // ephemeral.
  const char* src =
    "trait T1\n"
    "  fun f[A: Any val](x: A^)\n"

    "trait T2\n"
    "  fun f[A: Any val](x: A)\n"

    "class C is (T1 & T2)";

  const char* errs[] = {"clashing definitions for method 'f'", NULL};
  DO(test_expected_errors(src, "expr", errs));
}

TEST_F(TraitsTest, SpecializationGuardOnDifferentTypeParamIsNotSubtype)
{
  // The interface's specialization guards its first type parameter, A. C's
  // guards its second, which C also names A.
  const char* src =
    "interface I\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8 iftype A <: U32 val\n"

    "class C\n"
    "  fun f[B: Any val, A: Any val](a: B, b: A): U8 => 0\n"
    "  fun f[B: Any val, A: Any val](a: B, b: A): U8 iftype A <: U32 val => 1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let i: I = C\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, InheritedMethodPackageQualifiedNameNotRenamed)
{
  // Inside f, `R` is the method's type parameter and `l.R` is lib's
  // primitive. Only the former is renamed in C's copy.
  add_module("lib",
    "primitive R\n"
    "  fun v(): U8 => 7");

  const char* src =
    "use l = \"lib\"\n"

    "trait T\n"
    "  fun f[R: Any val](r: R): U8 =>\n"
    "    let g = {(): U8 => let x: l.R = l.R; x.v()}\n"
    "    g()\n"

    "class C[R: Any val] is T";

  TEST_COMPILE(src);
}

TEST_F(TraitsTest, SpecializationGuardOnDifferentTypeParamIsNotSubtypeInIsect)
{
  // Like SpecializationGuardOnDifferentTypeParamIsNotSubtype, through an
  // intersection that contains C.
  const char* src =
    "interface I\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8 iftype A <: U32 val\n"

    "interface J\n"
    "  fun g(): U8\n"

    "class C\n"
    "  fun f[B: Any val, A: Any val](a: B, b: A): U8 => 0\n"
    "  fun f[B: Any val, A: Any val](a: B, b: A): U8 iftype A <: U32 val => 1\n"
    "  fun g(): U8 => 2\n"

    "primitive P\n"
    "  fun apply(c: (C & J)) =>\n"
    "    let i: I = c\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  DO(test_expected_errors(src, "expr", errs));
}

TEST_F(TraitsTest, SpecializationGuardOnOtherTypeIsNotSubtype)
{
  // The guards test the same type parameter against different types.
  const char* src =
    "interface I\n"
    "  fun f[A: Any val](a: A): U8\n"
    "  fun f[A: Any val](a: A): U8 iftype A <: U32 val\n"

    "class C\n"
    "  fun f[A: Any val](a: A): U8 => 0\n"
    "  fun f[A: Any val](a: A): U8 iftype A <: U16 val => 1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let i: I = C\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, SpecializationMatchingGuardOtherSignatureNotSubtype)
{
  // D's specialization has J's guard, but returns a type J's doesn't allow.
  const char* src =
    "interface J\n"
    "  fun g[A: Any val](): (U8 | None)\n"
    "  fun g[A: Any val](): U8 iftype A <: U32 val\n"

    "class D\n"
    "  fun g[A: Any val](): (U8 | None) => None\n"
    "  fun g[A: Any val](): (U8 | None) iftype A <: U32 val => 1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let j: J = D\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'g' is missing a required specialization",
    "this specialization has the required guard but an incompatible "
      "signature",
    "None val is not a subtype of U8 val",
    "not every element of (U8 val | None val) is a subtype of U8 val",
    "method result (U8 val | None val) is not a subtype of U8 val",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, SpecializationGuardOnOtherTypeTwoTypeParamsIsNotSubtype)
{
  // The guards test the type parameter in the same position, with different
  // types.
  const char* src =
    "interface I\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8 iftype A <: U32 val\n"

    "class C\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8 => 0\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8 iftype A <: U16 val => 1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let i: I = C\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, SpecializationWithFewerTypeParamsIsNotSubtype)
{
  // Guards are matched by type parameter position, so the guard of a
  // specialization with a different number of type parameters isn't compared.
  const char* src =
    "interface I\n"
    "  fun f[A: Any val, B: Any val](a: A): U8\n"
    "  fun f[A: Any val, B: Any val](a: A): U8 iftype A <: U32 val\n"

    "class C\n"
    "  fun f[A: Any val](a: A): U8 => 0\n"
    "  fun f[A: Any val](a: A): U8 iftype A <: U32 val => 1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let i: I = C\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "methods have a different number of type parameters",
    "method 'f' has an incompatible signature",
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    "this specialization has a different number of type parameters, so its "
      "guard was not compared",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, AndGuardOnOtherTypeParamsIsNotSubtype)
{
  // Both of the inherited guard's clauses test B, while the interface's test
  // A and B. Both type parameters are narrowed to U32, but they are different
  // type parameters.
  const char* src =
    "interface I\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype A <: U32 val and B <: U32 val\n"

    "trait T\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8 => 0\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype B <: U32 val and B <: U32 val => 1\n"

    "class C is T\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let i: I = C\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, AndGuardClausesInOtherOrderIsSubtype)
{
  const char* src =
    "interface I\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype A <: U8 val and B <: U8 val\n"

    "class C\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8 => 0\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype B <: U8 val and A <: U8 val => 1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let i: I = C\n";

  TEST_COMPILE(src);
}

TEST_F(TraitsTest, AndGuardClausesWithDifferentTypesInOtherOrderIsSubtype)
{
  const char* src =
    "interface I\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype A <: U32 val and B <: U8 val\n"

    "class C\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8 => 0\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype B <: U8 val and A <: U32 val => 1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let i: I = C\n";

  TEST_COMPILE(src);
}

TEST_F(TraitsTest, SpecializationGuardOnClassTypeParamIsNotSubtype)
{
  // I's guard tests f's first type parameter. C's tests C's own type
  // parameter, which is U8 in C[U8], the type I's guard compares A against.
  const char* src =
    "interface I\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8 iftype A <: U8 val\n"

    "class C[A: Any val]\n"
    "  fun f[X: Any val, B: Any val](a: X, b: B): U8 => 0\n"
    "  fun f[X: Any val, B: Any val](a: X, b: B): U8 iftype A <: U8 val => 1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let i: I = C[U8]\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, SpecializationGuardOnTypeArgIsNotSubtype)
{
  // Both guards test the type's own type parameter, which is U32 for I and
  // String for C.
  const char* src =
    "interface I[T: Any val]\n"
    "  fun f(): U8\n"
    "  fun f(): U8 iftype T <: U32 val\n"

    "class C[T: Any val]\n"
    "  fun f(): U8 => 0\n"
    "  fun f(): U8 iftype T <: U32 val => 1\n"

    "primitive P\n"
    "  fun apply(c: C[String]) =>\n"
    "    let i: I[U32] = c\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, SpecializationGuardOnNarrowedTypeParamIsSubtype)
{
  // Inside the iftype, A is narrowed, and C[A] is an I[A].
  const char* src =
    "interface Showable\n"
    "  fun show(): U8\n"

    "interface I[T: Any val]\n"
    "  fun f(): U8\n"
    "  fun f(): U8 iftype T <: U32 val\n"

    "class C[T: Any val]\n"
    "  fun f(): U8 => 0\n"
    "  fun f(): U8 iftype T <: U32 val => 1\n"

    "primitive P\n"
    "  fun apply[A: Any val](c: C[A]) =>\n"
    "    iftype A <: Showable val then\n"
    "      let i: I[A] = c\n"
    "    end\n";

  TEST_COMPILE(src);
}

TEST_F(TraitsTest, SpecializationGuardOnOtherNarrowedTypeParamIsNotSubtype)
{
  // Inside the iftypes, c is a C[B], which is not an I[A].
  const char* src =
    "interface Showable\n"
    "  fun show(): U8\n"

    "interface I[T: Any val]\n"
    "  fun f(): U8\n"
    "  fun f(): U8 iftype T <: U32 val\n"

    "class C[T: Any val]\n"
    "  fun f(): U8 => 0\n"
    "  fun f(): U8 iftype T <: U32 val => 1\n"

    "primitive P\n"
    "  fun apply[A: Any val, B: Any val](c: C[B]) =>\n"
    "    iftype A <: Showable val then\n"
    "      iftype B <: Showable val then\n"
    "        let i: I[A] = c\n"
    "      end\n"
    "    end\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, SpecializationGuardOnTypeParamsNarrowedToSameTypeIsSubtype)
{
  // Inside the iftypes, A and B are both U32, so a C[B] is an I[A].
  const char* src =
    "interface I[T: Any val]\n"
    "  fun f(): U8\n"
    "  fun f(): U8 iftype T <: U32 val\n"

    "class C[T: Any val]\n"
    "  fun f(): U8 => 0\n"
    "  fun f(): U8 iftype T <: U32 val => 1\n"

    "primitive P\n"
    "  fun apply[A: Any val, B: Any val](c: C[B]) =>\n"
    "    iftype A <: U32 val then\n"
    "      iftype B <: U32 val then\n"
    "        let i: I[A] = c\n"
    "      end\n"
    "    end\n";

  TEST_COMPILE(src);
}

TEST_F(TraitsTest, SpecializationGuardOnEnclosingGuardNarrowingsIsSubtype)
{
  // In apply's specialization, A and B are both U32, so a C[B] is an I[A].
  const char* src =
    "interface I[T: Any val]\n"
    "  fun f(): U8\n"
    "  fun f(): U8 iftype T <: U8 val\n"

    "class C[T: Any val]\n"
    "  fun f(): U8 => 0\n"
    "  fun f(): U8 iftype T <: U8 val => 1\n"

    "primitive P\n"
    "  fun apply[A: Any val, B: Any val](c: C[B]): U8 => 0\n"
    "  fun apply[A: Any val, B: Any val](c: C[B]): U8\n"
    "    iftype A <: U32 val and B <: U32 val\n"
    "  =>\n"
    "    let i: I[A] = c\n"
    "    i.f()\n";

  TEST_COMPILE(src);
}

TEST_F(TraitsTest, SpecializationGuardOnMixedEnclosingNarrowingsIsSubtype)
{
  // A is narrowed by apply's guard and B by an iftype in its body. Both are
  // U32, so a C[B] is an I[A].
  const char* src =
    "interface I[T: Any val]\n"
    "  fun f(): U8\n"
    "  fun f(): U8 iftype T <: U8 val\n"

    "class C[T: Any val]\n"
    "  fun f(): U8 => 0\n"
    "  fun f(): U8 iftype T <: U8 val => 1\n"

    "primitive P\n"
    "  fun apply[A: Any val, B: Any val](c: C[B]): U8 => 0\n"
    "  fun apply[A: Any val, B: Any val](c: C[B]): U8 iftype A <: U32 val =>\n"
    "    iftype B <: U32 val then\n"
    "      let i: I[A] = c\n"
    "      i.f()\n"
    "    else\n"
    "      0\n"
    "    end\n";

  TEST_COMPILE(src);
}

TEST_F(TraitsTest, TupleGuardWithRenamedTypeParamsIsSubtype)
{
  const char* src =
    "interface I\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype (A, B) <: (U32 val, U32 val)\n"

    "class C\n"
    "  fun f[X: Any val, Y: Any val](a: X, b: Y): U8 => 0\n"
    "  fun f[X: Any val, Y: Any val](a: X, b: Y): U8\n"
    "    iftype (X, Y) <: (U32 val, U32 val) => 1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let i: I = C\n";

  TEST_COMPILE(src);
}

TEST_F(TraitsTest, TupleGuardOnTypeParamsInOtherOrderIsNotSubtype)
{
  // C's guard tests its second type parameter, then its first.
  const char* src =
    "interface I\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype (A, B) <: (U32 val, U32 val)\n"

    "class C\n"
    "  fun f[B: Any val, A: Any val](a: B, b: A): U8 => 0\n"
    "  fun f[B: Any val, A: Any val](a: B, b: A): U8\n"
    "    iftype (A, B) <: (U32 val, U32 val) => 1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let i: I = C\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, TupleGuardOnClassTypeParamIsNotSubtype)
{
  // C's guard tests C's own type parameter where I's tests f's first type
  // parameter.
  const char* src =
    "interface I\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype (A, B) <: (U8 val, U8 val)\n"

    "class C[Z: Any val]\n"
    "  fun f[X: Any val, Y: Any val](a: X, b: Y): U8 => 0\n"
    "  fun f[X: Any val, Y: Any val](a: X, b: Y): U8\n"
    "    iftype (Z, Y) <: (U8 val, U8 val) => 1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let i: I = C[U8]\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, OrGuardClausesInOtherOrderIsSubtype)
{
  const char* src =
    "interface val Named\n"
    "  fun name(): String\n"

    "interface val Sized\n"
    "  fun size(): USize\n"

    "interface I[A: Any val]\n"
    "  fun f(): U8\n"
    "  fun f(): U8 iftype A <: Named or A <: Sized\n"

    "class C[A: Any val]\n"
    "  fun f(): U8 => 0\n"
    "  fun f(): U8 iftype A <: Sized or A <: Named => 1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let i: I[String] = C[String]\n";

  TEST_COMPILE(src);
}

TEST_F(TraitsTest, TupleGuardWithDifferentArityIsNotSubtype)
{
  const char* src =
    "interface I\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype (A, B) <: (U32 val, U32 val)\n"

    "class C[Z: Any val]\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8 => 0\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype (A, B, Z) <: (U32 val, U32 val, U32 val) => 1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let i: I = C[U32]\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, SecondSpecializationMatchingGuardOtherSignatureNotSubtype)
{
  // D's second specialization has J's guard, but returns a type J's doesn't
  // allow. The first returns a different type, so the test fails if the first
  // is reported: the expected frames name (U8 val | None val).
  const char* src =
    "interface J\n"
    "  fun g[A: Any val](): (U8 | None)\n"
    "  fun g[A: Any val](): U8 iftype A <: U32 val\n"

    "class D\n"
    "  fun g[A: Any val](): (U8 | None) => None\n"
    "  fun g[A: Any val](): (U8 | String) iftype A <: U16 val => 2\n"
    "  fun g[A: Any val](): (U8 | None) iftype A <: U32 val => 1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let j: J = D\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'g' is missing a required specialization",
    "this specialization has the required guard but an incompatible "
      "signature",
    "None val is not a subtype of U8 val",
    "not every element of (U8 val | None val) is a subtype of U8 val",
    "method result (U8 val | None val) is not a subtype of U8 val",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, AndGuardWithFewerClausesIsNotSubtype)
{
  const char* src =
    "interface val Named\n"
    "  fun name(): String\n"

    "interface I\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype A <: U8 val and B <: U8 val and A <: Named\n"

    "class C\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8 => 0\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype B <: U8 val and A <: U8 val => 1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let i: I = C\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, SpecializationWithExtraDefaultedTypeParamIsNotSubtype)
{
  // D's specialization has I's guard but an extra type parameter.
  const char* src =
    "interface I\n"
    "  fun f[A: Any val](a: A): U8\n"
    "  fun f[A: Any val](a: A): U8 iftype A <: U8 val\n"

    "class D\n"
    "  fun f[B: Any val](b: B): U8 => 0\n"
    "  fun f[B: Any val, Z: Any val = None](b: B): U8 iftype B <: U8 val =>\n"
    "    1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let j: I = D\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    "this specialization has a different number of type parameters, so its "
      "guard was not compared",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, OrGuardOnOtherTypeParamsIsNotSubtype)
{
  // Inside apply, A and B are different type parameters, so C's guard, which
  // tests its second type parameter, is not I's, which tests its first.
  const char* src =
    "interface I[X: Any val, Y: Any val]\n"
    "  fun f(): U8\n"
    "  fun f(): U8 iftype X <: U32 val or X <: U16 val\n"

    "class C[X: Any val, Y: Any val]\n"
    "  fun f(): U8 => 0\n"
    "  fun f(): U8 iftype Y <: U16 val or Y <: U32 val => 1\n"

    "primitive P\n"
    "  fun apply[A: Any val, B: Any val](c: C[A, B]) =>\n"
    "    let i: I[A, B] = c\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, SpecializationWithExtraTypeParamAndOtherGuardIsNotSubtype)
{
  // D has no specialization with I's guard. Its only specialization tests
  // another type and has an extra type parameter.
  const char* src =
    "interface I\n"
    "  fun f[A: Any val](a: A): U8\n"
    "  fun f[A: Any val](a: A): U8 iftype A <: U16 val\n"

    "class D\n"
    "  fun f[B: Any val](b: B): U8 => 0\n"
    "  fun f[B: Any val, Z: Any val = None](b: B): U8 iftype B <: U32 val =>\n"
    "    1\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let j: I = D\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    "this specialization has a different number of type parameters, so its "
      "guard was not compared",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, SpecializationMatchingGuardAfterExtraTypeParamIsReported)
{
  // Both of D's specializations have I's guard. The first has an extra type
  // parameter, so its guard is not compared; the second returns a type I's
  // doesn't allow and is the one reported.
  const char* src =
    "interface I\n"
    "  fun f[A: Any val](a: A): (U8 | String)\n"
    "  fun f[A: Any val](a: A): U8 iftype A <: U8 val\n"

    "class D\n"
    "  fun f[B: Any val](b: B): (U8 | String) => 0\n"
    "  fun f[B: Any val, Z: Any val = None](b: B): (U8 | String)\n"
    "    iftype B <: U8 val => 1\n"
    "  fun f[B: Any val](b: B): (U8 | String) iftype B <: U8 val => \"x\"\n"

    "primitive P\n"
    "  fun apply() =>\n"
    "    let j: I = D\n";

  const char* errs[] = {"right side must be a subtype of left side", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "this specialization has the required guard but an incompatible "
      "signature",
    "String val is not a subtype of U8 val",
    "not every element of (U8 val | String val) is a subtype of U8 val",
    "method result (U8 val | String val) is not a subtype of U8 val",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, SpecializationGuardOnOtherPositionThanTraitNotProvided)
{
  // C's specialization guards its second type parameter, and T's its first,
  // so C doesn't have T's specialization.
  const char* src =
    "trait T\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8 => 0\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8 iftype A <: U32 val => 1\n"

    "class C is T\n"
    "  fun f[B: Any val, A: Any val](a: B, b: A): U8 => 10\n"
    "  fun f[B: Any val, A: Any val](a: B, b: A): U8\n"
    "    iftype A <: U32 val => 2\n";

  const char* errs[] = {"type does not implement its provides list", NULL};
  const char* frame0[] = {
    "method 'f' is missing a required specialization",
    "no specialization has this guard",
    NULL
  };
  const char** frames[] = {frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}

TEST_F(TraitsTest, OverrideWithRepeatedGuardClauseDoesNotReplace)
{
  // C's guard repeats one of T's clauses, so it isn't T's guard and doesn't
  // replace T's specialization. Both stay in the group, and C's guard covers
  // every type T's does, so T's is reported unreachable.
  const char* src =
    "trait T\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8 => 0\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype A <: U32 val and B <: U8 val => 1\n"

    "class C is T\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype A <: U32 val and A <: U32 val => 2\n";

  const char* errs[] = {
    "specialization is unreachable because a previous guard matches all the "
      "same types",
    NULL
  };

  DO(test_expected_errors(src, "verify", errs));
}

TEST_F(TraitsTest, OverrideWithFewerGuardClausesDoesNotReplace)
{
  // C's guard has two of T's three clauses, so it doesn't replace T's
  // specialization. Both stay in the group, and C's guard covers every type
  // T's does, so T's is reported unreachable.
  const char* src =
    "interface val Named\n"
    "  fun name(): String\n"

    "trait T\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8 => 0\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype A <: U32 val and B <: U8 val and A <: Named => 1\n"

    "class C is T\n"
    "  fun f[A: Any val, B: Any val](a: A, b: B): U8\n"
    "    iftype A <: U32 val and B <: U8 val => 2\n";

  const char* errs[] = {
    "specialization is unreachable because a previous guard matches all the "
      "same types",
    NULL
  };

  DO(test_expected_errors(src, "verify", errs));
}

TEST_F(TraitsTest, InheritedMethodObjectLiteralTypeUsesSourceName)
{
  // The object literal's type names the type argument it is created with,
  // which is the method's type parameter, in the trait's method and in C's
  // copy of it.
  const char* src =
    "interface Named\n"
    "  fun name(): U8\n"

    "trait T\n"
    "  fun make[R: Any val](r: R): U8 =>\n"
    "    let x: U8 = object is Named\n"
    "      fun name(): U8 => 0\n"
    "    end\n"
    "    x\n"

    "class C is T\n";

  const char* errs[] = {
    "right side must be a subtype of left side",
    "right side must be a subtype of left side",
    NULL
  };
  const char* frame0[] = {
    "object literal[R val] val^ is not a subtype of U8 val^", NULL
  };
  const char** frames[] = {frame0, frame0, NULL};

  DO(test_expected_error_frames(src, "expr", errs, frames));
}
