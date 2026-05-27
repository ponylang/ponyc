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


// A string literal whose closing quote is immediately followed by another
// quote starts a second string with no separator. Without a real newline or
// semicolon between them this is two expressions on the same line, even when
// the first string spans multiple source lines.
TEST_F(ParseExprTest, AdjacentStringLiteralsRequireSeparator)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print(\"\n"
    "      line\n"
    "    \"\"\")";

  TEST_ERRORS_1(src,
    "Use a semi colon to separate expressions on the same line");
}


// Same as above for two single-quoted strings on the same physical line.
TEST_F(ParseExprTest, SameLineAdjacentStringLiteralsRequireSeparator)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print(\"a\"\"b\")";

  TEST_ERRORS_1(src,
    "Use a semi colon to separate expressions on the same line");
}


// A multi-line string followed by an operator continues a single expression.
// Regression guard: the closing quote sits on a different source line from
// where the string started, but no actual newline separates it from the `+`.
TEST_F(ParseExprTest, MultilineStringFollowedByOperator)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print(\"\n"
    "      first\n"
    "    \" + \" second\")";

  TEST_COMPILE(src);
}


// A block comment containing newlines must not swallow the real newline that
// preceded it. The two `let`s here sit on different physical lines and should
// parse as two statements.
TEST_F(ParseExprTest, BlockCommentPreservesPrecedingNewline)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a: I32 = 5\n"
    "    /* multi\n"
    "    line\n"
    "    comment */ let b: I32 = 6\n"
    "    env.out.print((a + b).string())";

  TEST_COMPILE(src);
}
