#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "traits"))
#define TEST_ERROR(src) DO(test_error(src, "traits"))
#define TEST_EQUIV(src, expect) DO(test_equiv(src, "traits", expect, "traits"))


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
    "trait T is T";

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
    "  var y: Bool";

  TEST_COMPILE(src);
}


// Contra- and co-variance

TEST_F(TraitsTest, Contravariance)
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


TEST_F(TraitsTest, ReverseContravarianceFail)
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


TEST_F(TraitsTest, MultiContravarianceSelection)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(x: A)\n"

    "trait T2\n"
    "  fun f(x: AB)\n"

    "trait T3 is T1, T2\n"
    "  fun f(x: AB)";

  TEST_COMPILE(src);
}


TEST_F(TraitsTest, ReverseMultiContravarianceSelectionFail)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(x: A)\n"

    "trait T2\n"
    "  fun f(x: AB)\n"

    "trait T3 is T1, T2\n"
    "  fun f(x: A)";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, MultiContravarianceNoSelectionFail)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(x: A)\n"

    "trait T2\n"
    "  fun f(x: AB)\n"

    "trait T3 is T1, T2";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, MultiNovarianceNoSelection)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(x: A)\n"

    "trait T2\n"
    "  fun f(x: A)\n"

    "trait T3 is T1, T2";

  TEST_COMPILE(src);
}


TEST_F(TraitsTest, IndepenedentContravarianceSelection)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(x: A)\n"

    "trait T2\n"
    "  fun f(x: B)\n"

    "trait T3 is T1, T2\n"
    "  fun f(x: AB)";

  TEST_COMPILE(src);
}


TEST_F(TraitsTest, IndepenedentContravarianceNoSelectionFail)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(x: A)\n"

    "trait T2\n"
    "  fun f(x: B)\n"

    "trait T3 is T1, T2";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, Covariance)
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


TEST_F(TraitsTest, ReverseCovarianceFail)
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


TEST_F(TraitsTest, IndepenedentCovarianceNoSelectionFail)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(): A\n"

    "trait T2\n"
    "  fun f(): B\n"

    "trait T3 is T1, T2";

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

  WALK_TREE(package);
  LOOKUP("C", TK_CLASS);
  LOOKUP("f", TK_FUN);
  METHOD_BODY;
  CHILD(0);

  ASSERT_EQ(2, ast_int(walk_ast));
}


TEST_F(TraitsTest, NoBodyFail)
{
  const char* src =
    "trait T\n"
    "  fun f(): U32\n"
    "class C is T";

  TEST_ERROR(src);
}


TEST_F(TraitsTest, AmbiguousBodyFail)
{
  const char* src =
    "trait T1\n"
    "  fun f(): U32 => 1\n"
    "trait T2\n"
    "  fun f(): U32 => 2\n"
    "class C is T1, T2";

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

    "class C is T2, T3\n"
    "  new create() => true";

  const char* expect =
    "trait T1\n"
    "  fun f(): U32 => 1\n"

    "trait T2 is T1\n"
    "  fun f(): U32 => 1\n"

    "trait T3 is T1\n"
    "  fun f(): U32 => 1\n"

    "class C is T2, T3\n"
    "  new create() => true\n"
    "  fun f(): U32 => 1";

  TEST_EQUIV(src, expect);
}


TEST_F(TraitsTest, ContravariantBodyInheritted)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(x: A)\n"

    "trait T2\n"
    "  fun f(x: AB) => 1\n"

    "trait T3 is T1, T2\n"
    "  fun f(x: AB)";

  const char* expect =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(x: A)\n"

    "trait T2\n"
    "  fun f(x: AB) => 1\n"

    "trait T3 is T1, T2\n"
    "  fun f(x: AB) => 1";

  TEST_EQUIV(src, expect);
}


TEST_F(TraitsTest, ReverseContravariantBodyNotInheritted)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun f(x: A) => 1\n"

    "trait T2 is T1\n"
    "  fun f(x: AB)";

  TEST_COMPILE(src);

  WALK_TREE(package);
  LOOKUP("T2", TK_TRAIT);
  LOOKUP("f", TK_FUN);
  METHOD_BODY;

  ASSERT_EQ(TK_NONE, ast_id(walk_ast));
}
