#include <gtest/gtest.h>
#include "util.h"


// Parsing tests regarding expressions

#define TEST_ERROR(src) DO(test_error(src, "syntax"))
#define TEST_COMPILE(src) DO(test_compile(src, "syntax"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "syntax", errs)); }


class ParseExprTest : public PassTest
{};


// Lambda tests

TEST_F(ParseExprTest, Lambda)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {() => None }";

  TEST_COMPILE(src);
}


TEST_F(ParseExprTest, LambdaCap)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {iso() => None }";

  TEST_COMPILE(src);
}


TEST_F(ParseExprTest, LambdaCaptureVariable)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {()(x) => None }";

  TEST_COMPILE(src);
}


TEST_F(ParseExprTest, LambdaCaptureExpression)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {()(x = y) => None }";

  TEST_COMPILE(src);
}


TEST_F(ParseExprTest, LambdaCaptureExpressionAndType)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {()(x: T = y) => None }";

  TEST_COMPILE(src);
}


TEST_F(ParseExprTest, LambdaCaptureTypeWithoutExpressionFail)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {()(x: T) => None }";

  TEST_ERROR(src);
}


TEST_F(ParseExprTest, LambdaOldSyntax)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    lambda() => None end";

  TEST_ERRORS_1(src,
    "lambda ... end is no longer supported syntax; use {...} for lambdas");
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
