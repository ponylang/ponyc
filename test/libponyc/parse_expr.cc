#include <gtest/gtest.h>
#include "util.h"


// Parsing tests regarding expressions

#define TEST_AST(src, expect) DO(test_program_ast(src, "syntax", expect))
#define TEST_ERROR(src) DO(test_error(src, "syntax"))
#define TEST_COMPILE(src) DO(test_compile(src, "syntax"))


class ParseExprTest : public PassTest
{};




// Delegate tests
/*
TEST_F(ParseExprTest, Delegate)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    bar delegate T1";

  TEST_COMPILE(src);
}


TEST_F(ParseExprTest, DelegateCanBeIntersect)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    bar delegate (T1 & T2)";

  TEST_COMPILE(src);
}


TEST_F(ParseExprTest, DelegateCannotBeUnion)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    bar delegate (T1 | T2)";

  TEST_ERROR(src);
}


TEST_F(ParseExprTest, DelegateCannotBeTuple)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    bar delegate (T1, T2)";

  TEST_ERROR(src);
}


TEST_F(ParseExprTest, DelegateCannotBeArrow)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    bar delegate T1->T2";

  TEST_ERROR(src);
}


TEST_F(ParseExprTest, DelegateCannotHaveCapability)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    bar delegate T1 ref";

  TEST_ERROR(src);
}
*/


// Lambda tests

TEST_F(ParseExprTest, Lambda)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    lambda() => None end";

  TEST_COMPILE(src);
}


TEST_F(ParseExprTest, LambdaCaptureVariable)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    lambda()(x) => None end";

  TEST_COMPILE(src);
}


TEST_F(ParseExprTest, LambdaCaptureExpression)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    lambda()(x = y) => None end";

  TEST_COMPILE(src);
}


TEST_F(ParseExprTest, LambdaCaptureExpressionAndType)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    lambda()(x: T = y) => None end";

  TEST_COMPILE(src);
}


TEST_F(ParseExprTest, LambdaCaptureTypeWithoutExpressionFail)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    lambda()(x: T) => None end";

  TEST_ERROR(src);
}
