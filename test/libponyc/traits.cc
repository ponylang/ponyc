#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/builder.h>
#include <pass/traits.h>
#include <pkg/package.h>
#include <ast/stringtab.h>

#include "util.h"
#include "builtin_ast.h"


#define TEST_EXPECTED(src, expect) \
  DO(test_same(src, "traits", expect, "traits"))

#define TEST_BAD(src) \
  DO(test_fail(src, "traits"))


static const char* builtin2 =
  "primitive U32\n"
  "primitive None\n"
  "primitive Bool";


class TraitsTest : public testing::Test
{
protected:
  ast_t* program;
  ast_t* package;

  virtual void SetUp()
  {
    program = NULL;
  }

  virtual void TearDown()
  {
    ast_free(program);
    program = NULL;

    if(HasFatalFailure())
      print_errors();
  }

  void build(const char* src)
  {
    package_add_magic("builtin", builtin2);
    package_add_magic("prog", src);
    DO(load_test_program("traits", "prog", &program));
    package = ast_child(program);
  }
};


TEST_F(TraitsTest, ClassGetsTraitBody)
{
  const char* src =
    "trait T\n"
    "  fun ref bar(): U32 => 1\n"
    "class C is T\n"
    "  new create() => true";

  const char* expect =
    "trait T\n"
    "  fun ref bar(): U32 => 1\n"
    "class C is T\n"
    "  new create() => true\n"
    "  fun ref bar(): U32 => 1";

  TEST_EXPECTED(src, expect);
}


TEST_F(TraitsTest, ClassBodyNotOverriddenByTrait)
{
  const char* src =
    "trait T\n"
    "  fun ref bar(): U32 => 1\n"
    "class C is T\n"
    "  fun ref bar(): U32 => 2";

  DO(build(src));
  WALK_TREE(body, package,
    LOOKUP("C")
    LOOKUP("bar")
    CHILD(6)
    CHILD(0));

  ASSERT_EQ(2, ast_int(body));
}


TEST_F(TraitsTest, CannotProvideClass)
{
  const char* src =
    "class C1\n"
    "class C2 is C1";

  TEST_BAD(src);
}


TEST_F(TraitsTest, NoBody)
{
  const char* src =
    "trait T\n"
    "  fun ref bar(): U32\n"
    "class C is T";

  TEST_BAD(src);
}


TEST_F(TraitsTest, AmbiguousBody)
{
  const char* src =
    "trait T1\n"
    "  fun ref bar(): U32 => 1\n"
    "trait T2\n"
    "  fun ref bar(): U32 => 2\n"
    "class C is T1, T2";

  TEST_BAD(src);
}


TEST_F(TraitsTest, TransitiveTraits)
{
  const char* src =
    "trait T1\n"
    "  fun ref f2(): U32 => 2\n"

    "trait T2 is T1\n"
    "  fun ref f1(): Bool => 1\n"

    "class C is T2\n"
    "  new create() => true";

  const char* expect =
    "trait T1\n"
    "  fun ref f2(): U32 => 2\n"

    "trait T2 is T1\n"
    "  fun ref f1(): Bool => 1\n"
    "  fun ref f2(): U32 => 2\n"

    "class C is T2\n"
    "  new create() => true\n"
    "  fun ref f1(): Bool => 1\n"
    "  fun ref f2(): U32 => 2\n";

  TEST_EXPECTED(src, expect);
}


TEST_F(TraitsTest, SingleTraitLoop)
{
  const char* src =
    "trait T is T";

  TEST_BAD(src);
}


TEST_F(TraitsTest, MultiTraitLoop)
{
  const char* src =
    "trait T1 is T2\n"
    "trait T2 is T3\n"
    "trait T3 is T1\n";

  TEST_BAD(src);
}


TEST_F(TraitsTest, TraitAndClassNamesDontClash)
{
  const char* src =
    "trait T\n"
    "  fun ref bar(y: U32): U32 => y\n"
    "class C is T\n"
    "  var y: Bool";

  DO(build(src));
}


TEST_F(TraitsTest, DiamondInheritance)
{
  const char* src =
    "trait T1\n"
    "  fun ref bar(): U32 => 1\n"

    "trait T2 is T1\n"
    "trait T3 is T1\n"

    "class C is T2, T3\n"
    "  new create() => true";

  const char* expect =
    "trait T1\n"
    "  fun ref bar(): U32 => 1\n"

    "trait T2 is T1\n"
    "trait T3 is T1\n"

    "class C is T2, T3\n"
    "  new create() => true\n"
    "  fun ref bar(): U32 => 1\n";

  TEST_EXPECTED(src, expect);
}


TEST_F(TraitsTest, MethodContravariance)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun ref bar(x: A): U32 => 1\n"

    "trait T2 is T1\n"
    "  fun ref bar(x: AB): U32 => 2\n"
    
    "class C is T2\n"
    "  new create() => true";

  const char* expect =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun ref bar(x: A): U32 => 1\n"

    "trait T2 is T1\n"
    "  fun ref bar(x: AB): U32 => 2\n"

    "class C is T2\n"
    "  new create() => true\n"
    "  fun ref bar(x: AB): U32 => 2\n";

  TEST_EXPECTED(src, expect);
}


TEST_F(TraitsTest, MethodContravarianceMultiSource)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun ref bar(x: A): U32 => 1\n"

    "trait T2\n"
    "  fun ref bar(x: AB): U32 => 2\n"

    "class C is T1, T2\n"
    "  new create() => true";

  const char* expect =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun ref bar(x: A): U32 => 1\n"

    "trait T2\n"
    "  fun ref bar(x: AB): U32 => 2\n"

    "class C is T1, T2\n"
    "  new create() => true\n"
    "  fun ref bar(x: AB): U32 => 2\n";

  TEST_EXPECTED(src, expect);
}


TEST_F(TraitsTest, MethodContravarianceMultiSourceClash)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun ref bar(x: A): U32 => 1\n"

    "trait T2\n"
    "  fun ref bar(x: B): U32 => 2\n"

    "class C is T1, T2\n"
    "  new create() => true";

  TEST_BAD(src);
}


TEST_F(TraitsTest, MethodContravariance3Source)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun ref bar(x: A): U32 => 1\n"

    "trait T2\n"
    "  fun ref bar(x: B): U32 => 2\n"

    "trait T3\n"
    "  fun ref bar(x: AB): U32 => 3\n"

    "class C is T1, T2, T3\n"
    "  new create() => true";

  const char* expect =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun ref bar(x: A): U32 => 1\n"

    "trait T2\n"
    "  fun ref bar(x: B): U32 => 2\n"

    "trait T3\n"
    "  fun ref bar(x: AB): U32 => 3\n"

    "class C is T1, T2, T3\n"
    "  new create() => true\n"
    "  fun ref bar(x: AB): U32 => 3\n";

  TEST_EXPECTED(src, expect);
}


TEST_F(TraitsTest, MethodContravariance3SourceReverseOrder)
{

  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun ref bar(x: A): U32 => 1\n"

    "trait T2\n"
    "  fun ref bar(x: B): U32 => 2\n"

    "trait T3\n"
    "  fun ref bar(x: AB): U32 => 3\n"

    "class C is T3, T2, T1\n"
    "  new create() => true";

  const char* expect =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun ref bar(x: A): U32 => 1\n"

    "trait T2\n"
    "  fun ref bar(x: B): U32 => 2\n"

    "trait T3\n"
    "  fun ref bar(x: AB): U32 => 3\n"

    "class C is T3, T2, T1\n"
    "  new create() => true\n"
    "  fun ref bar(x: AB): U32 => 3\n";

  TEST_EXPECTED(src, expect);
}


TEST_F(TraitsTest, MethodReverseContravariance)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T\n"
    "  fun ref bar(x: AB): U32\n"

    "class C is T\n"
    "  new create() => true\n"
    "  fun ref bar(x: A): U32 => 2\n";

  TEST_BAD(src);
}


TEST_F(TraitsTest, MethodCovariance)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun ref bar(): A => 1\n"

    "trait T2\n"
    "  fun ref bar(): AB => 2\n"

    "class C is T1, T2\n"
    "  new create() => true";

  const char* expect =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T1\n"
    "  fun ref bar(): A => 1\n"

    "trait T2\n"
    "  fun ref bar(): AB => 2\n"

    "class C is T1, T2\n"
    "  new create() => true\n"
    "  fun ref bar(): A => 1\n";

  TEST_EXPECTED(src, expect);
}


TEST_F(TraitsTest, MethodReverseCovariance)
{
  const char* src =
    "trait A\n"
    "trait B\n"
    "type AB is (A | B)\n"

    "trait T\n"
    "  fun ref bar(): A\n"

    "class C is T\n"
    "  new create() => true\n"
    "  fun ref bar(): AB => 1\n";

  TEST_BAD(src);
}
