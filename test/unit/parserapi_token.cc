#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/parserapi.h"
#include "../../src/libponyc/ast/source.h"
#include "../../src/libponyc/ast/error.h"
PONY_EXTERN_C_END

#include <gtest/gtest.h>


DEF(token_dot);
  AST_NODE(TK_INT);
  TOKEN(NULL, TK_DOT);
  DONE();

DEF(token_multi);
  AST_NODE(TK_INT);
  TOKEN(NULL, TK_BANG, TK_COLON, TK_SEMI);
  DONE();

DEF(skip_dot);
  AST_NODE(TK_INT);
  SKIP(NULL, TK_DOT);
  DONE();

DEF(skip_multi);
  AST_NODE(TK_INT);
  SKIP(NULL, TK_BANG, TK_COLON, TK_SEMI);
  DONE();

DEF(optional_dot);
  AST_NODE(TK_INT);
  OPT TOKEN(NULL, TK_DOT);
  DONE();

DEF(optional_multi);
  AST_NODE(TK_INT);
  OPT TOKEN(NULL, TK_BANG, TK_COLON, TK_SEMI);
  DONE();


class ParserApiTokenTest: public testing::Test
{};


// TOKEN

TEST(ParserApiTokenTest, Token)
{
  const char* code = ".";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, token_dot);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_DOT, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiTokenTest, TokenMissing)
{
  const char* code = ":";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, token_dot);
  ASSERT_EQ(NULL, ast);

  source_close(src);
}


TEST(ParserApiTokenTest, TokenLexError)
{
  const char* code = "$";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, token_dot);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}


TEST(ParserApiTokenTest, TokenMultiFirst)
{
  const char* code = "!";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, token_multi);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_BANG, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiTokenTest, TokenMultiNotFirst)
{
  const char* code = ":";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, token_multi);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_COLON, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiTokenTest, TokenMultiMissing)
{
  const char* code = "3";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, token_multi);
  ASSERT_EQ(NULL, ast);

  source_close(src);
}


// SKIP

TEST(ParserApiTokenTest, Skip)
{
  const char* code = ".";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, skip_dot);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_EQ((void*)NULL, ast_child(ast));
  ASSERT_EQ((void*)NULL, ast_sibling(ast));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiTokenTest, SkipMissing)
{
  const char* code = "Foo";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, skip_dot);
  ASSERT_EQ(NULL, ast);

  source_close(src);
}


TEST(ParserApiTokenTest, SkipLexError)
{
  const char* code = "$";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, skip_dot);
  ASSERT_EQ(NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}


TEST(ParserApiTokenTest, SkipMultiFirst)
{
  const char* code = "!";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, skip_multi);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_EQ((void*)NULL, ast_child(ast));
  ASSERT_EQ((void*)NULL, ast_sibling(ast));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiTokenTest, SkipMultiNotFirst)
{
  const char* code = ":";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, skip_multi);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_EQ((void*)NULL, ast_child(ast));
  ASSERT_EQ((void*)NULL, ast_sibling(ast));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiTokenTest, SkipMultiMissing)
{
  const char* code = "Foo";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, skip_multi);
  ASSERT_EQ(NULL, ast);

  source_close(src);
}


// OPT TOKEN

TEST(ParserApiTokenTest, Optional)
{
  const char* code = ".";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, optional_dot);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_DOT, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiTokenTest, OptionalNotPresent)
{
  const char* code = ":";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, optional_dot);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_NONE, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiTokenTest, OptionalMultiFirst)
{
  const char* code = "!";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, optional_multi);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_BANG, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiTokenTest, OptionalMultiNotFirst)
{
  const char* code = ":";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, optional_multi);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_COLON, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiTokenTest, OptionalMultiNotPresent)
{
  const char* code = "3";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, optional_multi);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_NONE, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}
