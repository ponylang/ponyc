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
