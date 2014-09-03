#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/parserapi.h"
#include "../../src/libponyc/ast/source.h"
PONY_EXTERN_C_END

#include <gtest/gtest.h>


class ParserApiComplexTest: public testing::Test
{};


// Recursive rule

DEF(recurse);
  TOKEN(TK_COLON);
  IF(TK_PLUS, RULE(recurse));
  DONE();


TEST(ParserApiComplexTest, Recursion)
{
  const char* code = ":+:+:";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, recurse);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_COLON, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_COLON, ast_id(ast_child(ast)));
  ASSERT_NE((void*)NULL, ast_child(ast_child(ast)));
  ASSERT_EQ(TK_COLON, ast_id(ast_child(ast_child(ast))));
  ASSERT_NE((void*)NULL, ast_child(ast_child(ast_child(ast))));
  ASSERT_EQ(TK_NONE, ast_id(ast_child(ast_child(ast_child(ast)))));
  ASSERT_EQ((void*)NULL, ast_child(ast_child(ast_child(ast_child(ast)))));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiComplexTest, RecursionLexError)
{
  const char* code = ":+:+$";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, recurse);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}


TEST(ParserApiComplexTest, RecursionParseError)
{
  const char* code = ":+:+.";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, recurse);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}


// Mutually recursive rules

DECL(recurse_mut2);

DEF(recurse_mut1);
  TOKEN(TK_COLON);
  IF(TK_PLUS, RULE(recurse_mut2));
  DONE();

DEF(recurse_mut2);
  TOKEN(TK_SEMI);
  IF(TK_MINUS, RULE(recurse_mut1));
  DONE();

TEST(ParserApiComplexTest, MutualRecursion)
{
  const char* code = ":+;-:";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, recurse_mut1);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_COLON, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_SEMI, ast_id(ast_child(ast)));
  ASSERT_NE((void*)NULL, ast_child(ast_child(ast)));
  ASSERT_EQ(TK_COLON, ast_id(ast_child(ast_child(ast))));
  ASSERT_NE((void*)NULL, ast_child(ast_child(ast_child(ast))));
  ASSERT_EQ(TK_NONE, ast_id(ast_child(ast_child(ast_child(ast)))));
  ASSERT_EQ((void*)NULL, ast_child(ast_child(ast_child(ast_child(ast)))));
  ast_free(ast);

  source_close(src);
}
