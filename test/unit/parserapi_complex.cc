#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/parserapi.h"
#include "../../src/libponyc/ast/source.h"
PONY_EXTERN_C_END

#include "util.h"
#include <gtest/gtest.h>


class ParserApiComplexTest: public testing::Test
{};


// Recursive rule

DEF(recurse);
  TOKEN(NULL, TK_COLON);
  IF(TK_PLUS, RULE("test", recurse));
  DONE();


TEST(ParserApiComplexTest, Recursion)
{
  const char* code = ":+:+:";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, recurse, "test");
  DO(check_tree("(: (: (: x)))", ast));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiComplexTest, RecursionLexError)
{
  const char* code = ":+:+$";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, recurse, "test");
  ASSERT_EQ((void*)NULL, ast);

  source_close(src);
}


TEST(ParserApiComplexTest, RecursionParseError)
{
  const char* code = ":+:+.";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, recurse, "test");
  ASSERT_EQ((void*)NULL, ast);

  source_close(src);
}


// Mutually recursive rules

DECL(recurse_mut2);

DEF(recurse_mut1);
  TOKEN(NULL, TK_COLON);
  IF(TK_PLUS, RULE("test", recurse_mut2));
  DONE();

DEF(recurse_mut2);
  TOKEN(NULL, TK_SEMI);
  IF(TK_MINUS, RULE("test", recurse_mut1));
  DONE();

TEST(ParserApiComplexTest, MutualRecursion)
{
  const char* code = ":+;-:";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, recurse_mut1, "test");
  DO(check_tree("(: (; (: x)))", ast));

  ast_free(ast);
  source_close(src);
}
