/*
#include <gtest/gtest.h>
#include <platform.h>

#include <ast/parserapi.h>
#include <ast/source.h>

#include "util.h"

class ParserApiAstTest: public testing::Test
{};


DEF(token_only);
  TOKEN(NULL, TK_MOD);
  DONE();

TEST(ParserApiAstTest, TokenOnly)
{
  const char* code = "%";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, token_only, "test");
  DO(check_tree("(%)", ast));

  ast_free(ast);
  source_close(src);
}


DEF(ast_only);
  AST_NODE(TK_DOT);
  DONE();

TEST(ParserApiAstTest, AstOnly)
{
  const char* code = "";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, ast_only, "test");
  DO(check_tree("(.)", ast));

  ast_free(ast);
  source_close(src);
}


DEF(ast_then_token);
  AST_NODE(TK_DOT);
  TOKEN(NULL, TK_MOD);
  DONE();

TEST(ParserApiAstTest, AstThenToken)
{
  const char* code = "%";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, ast_then_token, "test");
  DO(check_tree("(. %)", ast));

  ast_free(ast);
  source_close(src);
}


DEF(token_then_ast);
  TOKEN(NULL, TK_MOD);
  AST_NODE(TK_DOT);
  DONE();

TEST(ParserApiAstTest, TokenThenAst)
{
  const char* code = "%";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, token_then_ast, "test");
  DO(check_tree("(% .)", ast));

  ast_free(ast);
  source_close(src);
}


DEF(seq_base);
  TOKEN(NULL, TK_MOD);
  DONE();

DEF(seq_then_ast);
  SEQ("test", seq_base);
  AST_NODE(TK_DOT);
  DONE();

TEST(ParserApiAstTest, SeqPresentThenAst)
{
  const char* code = "%";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, seq_then_ast, "test");
  DO(check_tree("(% .)", ast));

  ast_free(ast);
  source_close(src);
}

TEST(ParserApiAstTest, OptTokenMissingThenAst)
{
  const char* code = "";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, seq_then_ast, "test");
  DO(check_tree("(.)", ast));

  ast_free(ast);
  source_close(src);
}


DEF(null_base);
  SKIP(NULL, TK_PLUS);
  SKIP(NULL, TK_COLON);
  DONE();

DEF(null_top);
  AST_NODE(TK_DOT);
  RULE("test", null_base);
  DONE();

TEST(ParserApiAstTest, NullAst)
{
  const char* code = "+:";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, null_top, "test");
  DO(check_tree("(.)", ast));

  ast_free(ast);
  source_close(src);
}
*/