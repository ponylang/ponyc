#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/parserapi.h"
#include "../../src/libponyc/ast/source.h"
PONY_EXTERN_C_END

#include <gtest/gtest.h>


DEF(base);
  TOKEN(TK_DOT);
  TOKEN(TK_DOT);
  DONE();

DEF(base2);
  TOKEN(TK_COLON);
  DONE();

DEF(base3);
  TOKEN(TK_SEMI);
  DONE();

DEF(if_token);
  AST_NODE(TK_INT);
  IF(TK_PLUS, TOKEN(TK_MINUS));
  SKIP(TK_SEMI);  // Check next token isn't consumed when plus not present
  DONE();

DEF(if_rule);
  AST_NODE(TK_INT);
  IF(TK_PLUS, RULE(base));
  DONE();

DEF(if_block);
  AST_NODE(TK_INT);
  IF(TK_PLUS, TOKEN(TK_MINUS) RULE(base) TOKEN(TK_COLON));
  DONE();

DEF(while_token);
  AST_NODE(TK_INT);
  WHILE(TK_PLUS, TOKEN(TK_MINUS));
  SKIP(TK_SEMI);  // Check next token isn't consumed when plus not present
  DONE();

DEF(while_rule);
  AST_NODE(TK_INT);
  WHILE(TK_PLUS, RULE(base));
  DONE();

DEF(while_block);
  AST_NODE(TK_INT);
  WHILE(TK_PLUS, TOKEN(TK_MINUS) RULE(base) TOKEN(TK_COLON));
  DONE();

DEF(seq);
  AST_NODE(TK_INT);
  SEQ(base);
  DONE();

DEF(seq_multi);
  AST_NODE(TK_INT);
  SEQ(base, base2, base3);
  DONE();


class ParserApiCondTest: public testing::Test
{};


// IF

TEST(ParserApiCondTest, IfCondLexError)
{
  const char* code = "$";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, if_token);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}


TEST(ParserApiCondTest, IfToken)
{
  const char* code = "+-;";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, if_token);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_MINUS, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiCondTest, IfTokenMissing)
{
  const char* code = ";";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, if_token);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_NONE, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiCondTest, IfTokenLexError)
{
  const char* code = "+$";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, if_token);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}


TEST(ParserApiCondTest, IfRule)
{
  const char* code = "+..";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, if_rule);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_DOT, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiCondTest, IfRuleParseError)
{
  const char* code = "+.:";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, if_rule);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}


TEST(ParserApiCondTest, IfBlock)
{
  const char* code = "+-..:";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, if_block);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 0));
  ASSERT_EQ(TK_MINUS, ast_id(ast_childidx(ast, 0)));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 1));
  ASSERT_EQ(TK_DOT, ast_id(ast_childidx(ast, 1)));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 2));
  ASSERT_EQ(TK_COLON, ast_id(ast_childidx(ast, 2)));
  ASSERT_EQ((void*)NULL, ast_childidx(ast, 3));
  ast_free(ast);

  source_close(src);
}


// WHILE

TEST(ParserApiCondTest, WhileCondLexError)
{
  const char* code = "$";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, while_token);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}


TEST(ParserApiCondTest, WhileTokenNone)
{
  const char* code = ";";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, while_token);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_EQ((void*)NULL, ast_child(ast));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiCondTest, WhileTokenOne)
{
  const char* code = "+-;";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, while_token);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 0));
  ASSERT_EQ(TK_MINUS, ast_id(ast_childidx(ast, 0)));
  ASSERT_EQ((void*)NULL, ast_childidx(ast, 1));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiCondTest, WhileTokenMany)
{
  const char* code = "+-+-+-;";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, while_token);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 0));
  ASSERT_EQ(TK_MINUS, ast_id(ast_childidx(ast, 0)));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 1));
  ASSERT_EQ(TK_MINUS, ast_id(ast_childidx(ast, 1)));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 2));
  ASSERT_EQ(TK_MINUS, ast_id(ast_childidx(ast, 2)));
  ASSERT_EQ((void*)NULL, ast_childidx(ast, 3));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiCondTest, WhileTokenLexError)
{
  const char* code = "+-+$+-;";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, while_token);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}


TEST(ParserApiCondTest, WhileRule)
{
  const char* code = "+..";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, while_rule);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 0));
  ASSERT_EQ(TK_DOT, ast_id(ast_childidx(ast, 0)));
  ASSERT_EQ((void*)NULL, ast_childidx(ast, 1));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiCondTest, WhileRuleParseError)
{
  const char* code = "+.:";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, while_rule);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}


TEST(ParserApiCondTest, WhileBlock)
{
  const char* code = "+-..:";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, while_block);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 0));
  ASSERT_EQ(TK_MINUS, ast_id(ast_childidx(ast, 0)));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 1));
  ASSERT_EQ(TK_DOT, ast_id(ast_childidx(ast, 1)));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 2));
  ASSERT_EQ(TK_COLON, ast_id(ast_childidx(ast, 2)));
  ASSERT_EQ((void*)NULL, ast_childidx(ast, 3));
  ast_free(ast);

  source_close(src);
}


// SEQ

TEST(ParserApiCondTest, SeqSingleNone)
{
  const char* code = ";";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, seq);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_EQ((void*)NULL, ast_child(ast));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiCondTest, SeqSingleOne)
{
  const char* code = "..";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, seq);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 0));
  ASSERT_EQ(TK_DOT, ast_id(ast_childidx(ast, 0)));
  ASSERT_EQ((void*)NULL, ast_childidx(ast, 1));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiCondTest, SeqSingleMany)
{
  const char* code = "......";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, seq);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 0));
  ASSERT_EQ(TK_DOT, ast_id(ast_childidx(ast, 0)));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 1));
  ASSERT_EQ(TK_DOT, ast_id(ast_childidx(ast, 1)));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 2));
  ASSERT_EQ(TK_DOT, ast_id(ast_childidx(ast, 2)));
  ASSERT_EQ((void*)NULL, ast_childidx(ast, 3));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiCondTest, SeqMultiNone)
{
  const char* code = "";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, seq_multi);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_EQ((void*)NULL, ast_child(ast));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiCondTest, SeqMultiFirst)
{
  const char* code = "..";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, seq_multi);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 0));
  ASSERT_EQ(TK_DOT, ast_id(ast_childidx(ast, 0)));
  ASSERT_EQ((void*)NULL, ast_childidx(ast, 1));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiCondTest, SeqMultiNotFirst)
{
  const char* code = ";";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, seq_multi);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 0));
  ASSERT_EQ(TK_SEMI, ast_id(ast_childidx(ast, 0)));
  ASSERT_EQ((void*)NULL, ast_childidx(ast, 1));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiCondTest, SeqMultiMany)
{
  const char* code = ":..;";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, seq_multi);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 0));
  ASSERT_EQ(TK_COLON, ast_id(ast_childidx(ast, 0)));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 1));
  ASSERT_EQ(TK_DOT, ast_id(ast_childidx(ast, 1)));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 2));
  ASSERT_EQ(TK_SEMI, ast_id(ast_childidx(ast, 2)));
  ASSERT_EQ((void*)NULL, ast_childidx(ast, 3));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiCondTest, SeqMultiRepeated)
{
  const char* code = ":....:;";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, seq_multi);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 0));
  ASSERT_EQ(TK_COLON, ast_id(ast_childidx(ast, 0)));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 1));
  ASSERT_EQ(TK_DOT, ast_id(ast_childidx(ast, 1)));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 2));
  ASSERT_EQ(TK_DOT, ast_id(ast_childidx(ast, 2)));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 3));
  ASSERT_EQ(TK_COLON, ast_id(ast_childidx(ast, 3)));
  ASSERT_NE((void*)NULL, ast_childidx(ast, 4));
  ASSERT_EQ(TK_SEMI, ast_id(ast_childidx(ast, 4)));
  ASSERT_EQ((void*)NULL, ast_childidx(ast, 5));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiCondTest, SeqLexError)
{
  const char* code = ":$";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, seq_multi);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}


TEST(ParserApiCondTest, SeqParseError)
{
  const char* code = ":.:;";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, seq_multi);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}
