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


// Statement sequences.
//
// Sequences are parsed iteratively rather than right-recursively so that parser
// stack depth does not grow with the number of statements (issue #3660). These
// tests pin the observable behaviour the rewrite must preserve.

static bool ast_subtree_has_id(ast_t* ast, token_id id)
{
  if(ast == NULL)
    return false;

  if(ast_id(ast) == id)
    return true;

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
  {
    if(ast_subtree_has_id(child, id))
      return true;
  }

  return false;
}

TEST_F(ParseExprTest, NewlineSeparatedStatements)
{
  const char* src =
    "class Foo\n"
    "  fun m(): U64 =>\n"
    "    let a: U64 = 1\n"
    "    let b: U64 = 2\n"
    "    a + b";

  TEST_COMPILE(src);

  // Newline-separated statements produce no semicolon node.
  ASSERT_FALSE(ast_subtree_has_id(module, TK_SEMI));
}

TEST_F(ParseExprTest, SemicolonSeparatedStatements)
{
  const char* src =
    "class Foo\n"
    "  fun m(): U64 =>\n"
    "    let a: U64 = 1; let b: U64 = 2\n"
    "    a + b";

  TEST_COMPILE(src);

  // An explicit semicolon is preserved as a node in the sequence.
  ASSERT_TRUE(ast_subtree_has_id(module, TK_SEMI));
}

TEST_F(ParseExprTest, UnexpectedSemicolonBeforeNewline)
{
  const char* src =
    "class Foo\n"
    "  fun m(): U64 =>\n"
    "    let a: U64 = 1;\n"
    "    a";

  TEST_ERRORS_1(src,
    "Unexpected semicolon, only use to separate expressions on the same line");
}

TEST_F(ParseExprTest, MissingSemicolonBetweenSameLineStatements)
{
  const char* src =
    "class Foo\n"
    "  fun m(): U64 =>\n"
    "    let a: U64 = 1 let b: U64 = 2\n"
    "    a + b";

  TEST_ERRORS_1(src,
    "Use a semi colon to separate expressions on the same line");
}

TEST_F(ParseExprTest, JumpTerminatesSequence)
{
  const char* src =
    "class Foo\n"
    "  fun m(): U64 =>\n"
    "    let a: U64 = 1\n"
    "    return a";

  TEST_COMPILE(src);
}

// A jump ends its sequence, so a semicolon after one is a syntax error rather
// than the start of another statement. The return takes no value (the
// semicolon stops it absorbing one), so the trailing "; a" is left over.
TEST_F(ParseExprTest, SemicolonAfterJumpRejected)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    let a: U64 = 1; return; a";

  TEST_ERRORS_1(src, "unexpected token ;");
}

// Termination applies to every jump keyword, not just return. error ends its
// sequence too, so the trailing "; a" is left over and rejected.
TEST_F(ParseExprTest, SemicolonAfterErrorRejected)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    let a: U64 = 1; error; a";

  TEST_ERRORS_1(src, "unexpected token ;");
}

// break terminates its sequence, so the trailing "; a" is left over and the
// enclosing loop never reaches its end keyword.
TEST_F(ParseExprTest, SemicolonAfterBreakRejected)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    while true do break; a end";

  TEST_ERRORS_1(src, "unterminated while loop");
}

// The same iterative loop drives an annotated sequence (a try/match else body).
// A jump terminates it too, so the trailing "; a" is left over and the try's
// end keyword is never reached.
TEST_F(ParseExprTest, JumpTerminatesAnnotatedSequence)
{
  const char* src =
    "class Foo\n"
    "  fun m() =>\n"
    "    try error else return; a end";

  TEST_ERRORS_1(src, "unterminated try expression");
}

// A parenthesised expression is a single-statement sequence whose ')' sits on
// the same line, so the sequence parse sets a pending missing-semicolon flag
// that must be cleared. Without the clear the ')' (or a following token) gets
// the flag and a spurious "use a semi colon" error fires.
TEST_F(ParseExprTest, ParenthesisedSequenceDoesNotLeakMissingSemicolon)
{
  const char* src =
    "class Foo\n"
    "  fun m(): U64 =>\n"
    "    (1)";

  TEST_COMPILE(src);
}

// Regression test for #3660: a long statement sequence must not overflow the
// parser stack. The old right-recursive grammar recursed once per statement; a
// regression to it fails this test by overflowing the stack (a crash that
// fails the run), not by a clean assertion. 50000 is well above the old
// overflow point at the default test stack size.
TEST_F(ParseExprTest, LongSequenceDoesNotOverflowParser)
{
  std::string src =
    "class Foo\n"
    "  fun m() =>\n"
    "    var x: U64 = 0\n";

  for(size_t i = 0; i < 50000; i++)
    src += "    x = x + 1\n";

  TEST_COMPILE(src.c_str());
}
