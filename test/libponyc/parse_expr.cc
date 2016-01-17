#include <gtest/gtest.h>
#include "util.h"


// Parsing tests regarding expressions

//#define TEST_AST(src, expect) DO(test_program_ast(src, "syntax", expect))
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


TEST_F(ParseExprTest, LambdaCap)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    lambda iso() => None end";

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


// Compile error

TEST_F(ParseExprTest, CompileErrorAllowedAsIfdefClause)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    ifdef debug then\n"
    "      compile_error \"Reason\"\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(ParseExprTest, CompileErrorNeedsReason)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    ifdef debug then\n"
    "      compile_error\n"
    "    end";

  TEST_ERROR(src);
}


TEST_F(ParseExprTest, CompileErrorReasonMustBeAString)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    ifdef debug then\n"
    "      compile_error 34\n"
    "    end";

  TEST_ERROR(src);
}


TEST_F(ParseExprTest, ExpressionNotAllowedBeforeCompileError)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    ifdef debug then\n"
    "      let x = 3\n"
    "      compile_error \"Reason\"\n"
    "    end";

  TEST_ERROR(src);
}


TEST_F(ParseExprTest, ExpressionNotAllowedAfterCompileError)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    ifdef debug then\n"
    "      compile_error \"Reason\"\n"
    "      let x = 3\n"
    "    end";

  TEST_ERROR(src);
}


TEST_F(ParseExprTest, CompileErrorAllowedAsIfdefElseClause)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    ifdef debug then\n"
    "      let x = 3\n"
    "    else\n"
    "      compile_error \"Reason\"\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(ParseExprTest, CompileErrorNotAllowedOutsideIfdef)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    compile_error \"Reason\"";

  TEST_ERROR(src);
}

TEST_F(ParseExprTest, CompileErrorMissingPatternType)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    let a: U8 = 'a'\n"
    "    match a\n"
    "    | let c => None\n"
    "    end";

  TEST_COMPILE(src);
  //TEST_AST(src, "");
}
