#include <gtest/gtest.h>
#include <string.h>
#include <ast/error.h>
#include "util.h"


// Parsing tests regarding expressions

#define TEST_ERROR(src) DO(test_error(src, "syntax"))
#define TEST_COMPILE(src) DO(test_compile(src, "syntax"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "syntax", errs)); }

#define TEST_ERRORS_2(src, err1, err2) \
  { const char* errs[] = {err1, err2, NULL}; \
    DO(test_expected_errors(src, "syntax", errs)); }

// Recovery diagnostic for `id: Type` written where a `let` or `var` is required.
static const char* const missing_decl =
  "a variable declaration requires 'let' or 'var'";


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


// Match captures without let or var (issue #1479).
//
// `| tally: U64 => ...` used to leave the colon unparsed, so the compiler
// reported an unterminated match at the enclosing `match`. The identifier is
// still not a declaration; the syntax pass rejects it where the name is
// written.

static void assert_error_at(errors_t* errors, size_t index, size_t line,
  size_t pos)
{
  errormsg_t* error = errors_get_first(errors);

  for(size_t i = 0; i < index; i++)
  {
    ASSERT_NE((void*)NULL, error);
    error = error->next;
  }

  ASSERT_NE((void*)NULL, error);
  ASSERT_EQ(line, error->line);
  ASSERT_EQ(pos, error->pos);
}


static void assert_messages_lack(errors_t* errors, const char* fragment)
{
  for(errormsg_t* error = errors_get_first(errors); error != NULL;
    error = error->next)
  {
    ASSERT_TRUE(strstr(error->msg, fragment) == NULL) << error->msg;

    for(errormsg_t* frame = error->frame; frame != NULL; frame = frame->frame)
      ASSERT_TRUE(strstr(frame->msg, fragment) == NULL) << frame->msg;
  }
}


static bool references_have_one_child(ast_t* ast)
{
  if(ast == NULL)
    return true;

  if((ast_id(ast) == TK_REFERENCE) && (ast_childcount(ast) != 1))
    return false;

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
  {
    if(!references_have_one_child(child))
      return false;
  }

  return true;
}


static ast_t* find_named(ast_t* ast, token_id kind, const char* name)
{
  if(ast == NULL)
    return NULL;

  if(ast_id(ast) == kind)
  {
    ast_t* id = ast_child(ast);

    if((id != NULL) && (ast_id(id) == TK_ID) &&
      (strcmp(ast_name(id), name) == 0))
      return ast;
  }

  for(ast_t* child = ast_child(ast); child != NULL; child = ast_sibling(child))
  {
    ast_t* found = find_named(child, kind, name);

    if(found != NULL)
      return found;
  }

  return NULL;
}


// Full actor, with an unrelated identifier and type, matching the shape that
// used to be reported as an unterminated match.
TEST_F(ParseExprTest, MatchCaptureMissingDeclaration)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let value: (String | U64) = U64(1)\n"
    "    match value\n"
    "    | tally: U64 => env.out.print(tally.string())\n"
    "    | let text: String => env.out.print(text)\n"
    "    end";

  TEST_ERRORS_1(src, missing_decl);
  DO(assert_error_at(opt.check.errors, 0, 5, 7));
  DO(assert_messages_lack(opt.check.errors, "unterminated"));
}


TEST_F(ParseExprTest, GuardedMatchCaptureMissingDeclaration)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match env\n"
    "    | tally: U64 if true => tally\n"
    "    end";

  TEST_ERRORS_1(src, missing_decl);
  DO(assert_error_at(opt.check.errors, 0, 4, 7));
  DO(assert_messages_lack(opt.check.errors, "unterminated"));
}


TEST_F(ParseExprTest, ParenthesisedMatchCaptureMissingDeclaration)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match env\n"
    "    | (tally: U64) => tally\n"
    "    end";

  TEST_ERRORS_1(src, missing_decl);
  DO(assert_error_at(opt.check.errors, 0, 4, 8));
  DO(assert_messages_lack(opt.check.errors, "unterminated"));
}


TEST_F(ParseExprTest, TupleMatchCaptureMissingDeclaration)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match env\n"
    "    | (let alpha: U64, beta: Bool) => alpha\n"
    "    end";

  TEST_ERRORS_1(src, missing_decl);
  DO(assert_error_at(opt.check.errors, 0, 4, 24));
  DO(assert_messages_lack(opt.check.errors, "unterminated"));
}


TEST_F(ParseExprTest, NestedTupleMatchCaptureMissingDeclaration)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match env\n"
    "    | (let alpha: U64, (beta: Bool, let gamma: U64)) => alpha\n"
    "    end";

  TEST_ERRORS_1(src, missing_decl);
  DO(assert_error_at(opt.check.errors, 0, 4, 25));
  DO(assert_messages_lack(opt.check.errors, "unterminated"));
}


TEST_F(ParseExprTest, MultipleMatchCapturesMissingDeclaration)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match env\n"
    "    | (alpha: U64, beta: Bool) => alpha\n"
    "    end";

  TEST_ERRORS_2(src, missing_decl, missing_decl);
  DO(assert_error_at(opt.check.errors, 0, 4, 8));
  DO(assert_error_at(opt.check.errors, 1, 4, 20));
  DO(assert_messages_lack(opt.check.errors, "unterminated"));
}


TEST_F(ParseExprTest, MatchCaptureMissingDeclarationUnionType)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match env\n"
    "    | tally: (U64 | Bool) => tally\n"
    "    end";

  TEST_ERRORS_1(src, missing_decl);
  DO(assert_error_at(opt.check.errors, 0, 4, 7));
  DO(assert_messages_lack(opt.check.errors, "unterminated"));
}


TEST_F(ParseExprTest, MatchCaptureMissingDeclarationIntersectionType)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match env\n"
    "    | tally: (U64 & Bool) => tally\n"
    "    end";

  TEST_ERRORS_1(src, missing_decl);
  DO(assert_error_at(opt.check.errors, 0, 4, 7));
  DO(assert_messages_lack(opt.check.errors, "unterminated"));
}


TEST_F(ParseExprTest, MatchCaptureMissingDeclarationQualifiedType)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match env\n"
    "    | tally: builtin.U64 => tally\n"
    "    end";

  TEST_ERRORS_1(src, missing_decl);
  DO(assert_error_at(opt.check.errors, 0, 4, 7));
  DO(assert_messages_lack(opt.check.errors, "unterminated"));
}


// The shared reference rule also accepts this outside a match. It is still
// invalid, and the diagnostic stays about a declaration rather than a match.
TEST_F(ParseExprTest, TypedIdentifierOutsideMatchRequiresDeclaration)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    tally: U64";

  TEST_ERRORS_1(src, missing_decl);
  DO(assert_error_at(opt.check.errors, 0, 3, 5));
  DO(assert_messages_lack(opt.check.errors, "match"));
}


TEST_F(ParseExprTest, ValidMatchPatternsKeepReferenceShape)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match env\n"
    "    | let tally: U64 => tally\n"
    "    | var flag: Bool => flag\n"
    "    | (let left: U64, let right: Bool) => left\n"
    "    | (let outer: U64, (let inner: Bool, let third: U64)) => outer\n"
    "    | env => env\n"
    "    | _ => env\n"
    "    | 1 => env\n"
    "    | \"hi\" => env\n"
    "    | env.create() => env\n"
    "    | let guarded: U64 if true => guarded\n"
    "    else\n"
    "      env\n"
    "    end";

  TEST_COMPILE(src);
  ASSERT_TRUE(references_have_one_child(module));

  ast_t* tally = find_named(module, TK_LET, "tally");
  ASSERT_NE((void*)NULL, tally);
  ASSERT_NE(TK_NONE, ast_id(ast_childidx(tally, 1)));

  ast_t* flag = find_named(module, TK_VAR, "flag");
  ASSERT_NE((void*)NULL, flag);
  ASSERT_NE(TK_NONE, ast_id(ast_childidx(flag, 1)));
}


// The optional suffix lives on the shared reference rule, so these established
// forms must keep parsing and must not grow a child on a bare reference.
TEST_F(ParseExprTest, OrdinaryReferencesAndDeclarationsUnchanged)
{
  const char* src =
    "class Foo\n"
    "  fun m(tally: U64): Array[U64] =>\n"
    "    let xs: Array[U64] = [as U64: 1; 2; 3]\n"
    "    let captured = {()(inner: U64 = tally) => inner }\n"
    "    tally\n"
    "    tally.string\n"
    "    tally.string()\n"
    "    xs";

  TEST_COMPILE(src);
  ASSERT_TRUE(references_have_one_child(module));

  ast_t* param = find_named(module, TK_PARAM, "tally");
  ASSERT_NE((void*)NULL, param);
  ASSERT_NE(TK_NONE, ast_id(ast_childidx(param, 1)));

  ast_t* capture = find_named(module, TK_LAMBDACAPTURE, "inner");
  ASSERT_NE((void*)NULL, capture);
  ASSERT_NE(TK_NONE, ast_id(ast_childidx(capture, 1)));
  ASSERT_NE(TK_NONE, ast_id(ast_childidx(capture, 2)));
}


TEST_F(ParseExprTest, MissingMatchEndStillErrors)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match env\n"
    "    | let tally: U64 => tally";

  TEST_ERRORS_1(src, "unterminated match");
  DO(assert_messages_lack(opt.check.errors, missing_decl));
}


TEST_F(ParseExprTest, ColonWithoutTypeStillErrors)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match env\n"
    "    | tally: => tally\n"
    "    end";

  TEST_ERROR(src);
  DO(assert_messages_lack(opt.check.errors, missing_decl));
}


TEST_F(ParseExprTest, MalformedCaptureTypeStillErrors)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    match env\n"
    "    | tally: 1 => tally\n"
    "    end";

  TEST_ERROR(src);
  DO(assert_messages_lack(opt.check.errors, missing_decl));
}
