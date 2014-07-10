extern "C" {
#include "../../src/libponyc/ast/parserapi.h"
#include "../../src/libponyc/ast/source.h"
}
#include <gtest/gtest.h>


class ParserApiAstTest: public testing::Test
{};


DEF(token_only);
  TOKEN(TK_MOD);
  DONE();

TEST(ParserApiAstTest, TokenOnly)
{
  const char* code = "%";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, token_only);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_MOD, ast_id(ast));
  ASSERT_EQ((void*)NULL, ast_child(ast));
  ast_free(ast);

  source_close(src);
}


DEF(ast_only);
  AST_NODE(TK_INT);
  DONE();

TEST(ParserApiAstTest, AstOnly)
{
  const char* code = "";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, ast_only);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_EQ((void*)NULL, ast_child(ast));
  ast_free(ast);

  source_close(src);
}


DEF(ast_then_token);
  AST_NODE(TK_INT);
  TOKEN(TK_MOD);
  DONE();

TEST(ParserApiAstTest, AstThenToken)
{
  const char* code = "%";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, ast_then_token);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_MOD, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}


DEF(token_then_ast);
  TOKEN(TK_MOD);
  AST_NODE(TK_INT);
  DONE();

TEST(ParserApiAstTest, TokenThenAst)
{
  const char* code = "%";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, token_then_ast);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_MOD, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_INT, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}


DEF(seq_base);
  TOKEN(TK_MOD);
  DONE();

DEF(seq_then_ast);
  SEQ(seq_base);
  AST_NODE(TK_INT);
  DONE();

TEST(ParserApiAstTest, SeqPresentThenAst)
{
  const char* code = "%";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, seq_then_ast);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_MOD, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_INT, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}

TEST(ParserApiAstTest, OptTokenMissingThenAst)
{
  const char* code = "";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, seq_then_ast);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_EQ((void*)NULL, ast_child(ast));
  ast_free(ast);

  source_close(src);
}


DEF(null_base);
  SKIP(TK_PLUS);
  SKIP(TK_COLON);
  DONE();

DEF(null_top);
  AST_NODE(TK_INT);
  RULE(null_base);
  DONE();

TEST(ParserApiAstTest, NullAst)
{
  const char* code = "+:";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, null_top);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_EQ((void*)NULL, ast_child(ast));
  ASSERT_EQ((void*)NULL, ast_sibling(ast));
  ast_free(ast);

  source_close(src);
}