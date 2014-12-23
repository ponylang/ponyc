#include <gtest/gtest.h>
#include <platform.h>

#include <ast/parserapi.h>
#include <ast/source.h>
#include <ast/error.h>

#include "util.h"

/*
static bool _reached_end;
static bool _opt_at_end;
static bool _top_at_end;
static token_id _next_token_at_end;


DEF(token_test);
  AST_NODE(TK_COLON);
  TOKEN(NULL, TK_PLUS, TK_MULTIPLY, TK_AT);
  _reached_end = true;
  _opt_at_end = state.opt;
  _top_at_end = state.top;
  _next_token_at_end = current_token_id(parser);
  DONE();


DEF(token_opt_test);
  AST_NODE(TK_COLON);
  OPT TOKEN(NULL, TK_PLUS, TK_MULTIPLY, TK_AT);
  _reached_end = true;
  _opt_at_end = state.opt;
  _top_at_end = state.top;
  _next_token_at_end = current_token_id(parser);
  DONE();


DEF(token_top_test);
  AST_NODE(TK_COLON);
  TOP TOKEN(NULL, TK_PLUS, TK_MULTIPLY, TK_AT);
  _reached_end = true;
  _opt_at_end = state.opt;
  _top_at_end = state.top;
  _next_token_at_end = current_token_id(parser);
  DONE();


DEF(token_opt_top_test);
  AST_NODE(TK_COLON);
  OPT TOP TOKEN(NULL, TK_PLUS, TK_MULTIPLY, TK_AT);
  _reached_end = true;
  _opt_at_end = state.opt;
  _top_at_end = state.top;
  _next_token_at_end = current_token_id(parser);
  DONE();


DEF(skip_test);
  AST_NODE(TK_COLON);
  SKIP(NULL, TK_PLUS, TK_MULTIPLY, TK_AT);
  _reached_end = true;
  _opt_at_end = state.opt;
  _top_at_end = state.top;
  _next_token_at_end = current_token_id(parser);
  DONE();


DEF(skip_opt_test);
  AST_NODE(TK_COLON);
  OPT SKIP(NULL, TK_PLUS, TK_MULTIPLY, TK_AT);
  _reached_end = true;
  _opt_at_end = state.opt;
  _top_at_end = state.top;
  _next_token_at_end = current_token_id(parser);
  DONE();


static void check_end_state(token_id expect_token)
{
  ASSERT_TRUE(_reached_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_FALSE(_top_at_end);
  ASSERT_EQ(expect_token, _next_token_at_end);
}


class ParserApiTokenTest: public testing::Test
{};


// TOKEN

TEST(ParserApiTokenTest, TokenLexError)
{
  const char* code = "$";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_test, "test");
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiTokenTest, TokenMissing)
{
  const char* code = ".";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_test, "test");
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiTokenTest, TokenFirstPresent)
{
  const char* code = "+;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_test, "test");
  DO(check_tree("(: +)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiTokenTest, TokenNotFirstPresent)
{
  const char* code = "*;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_test, "test");
  DO(check_tree("(: *)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiTokenTest, TokenLastPresent)
{
  const char* code = "@;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_test, "test");
  DO(check_tree("(: @)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


// OPT TOKEN

TEST(ParserApiTokenTest, TokenOptLexError)
{
  const char* code = "$";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_opt_test, "test");
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiTokenTest, TokenOptMissing)
{
  const char* code = ".";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_opt_test, "test");
  DO(check_tree("(: x)", ast));
  DO(check_end_state(TK_DOT));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiTokenTest, TokenOptFirstPresent)
{
  const char* code = "+;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_opt_test, "test");
  DO(check_tree("(: +)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


// TOP TOKEN

TEST(ParserApiTokenTest, TokenTopFirstPresent)
{
  const char* code = "+;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_top_test, "test");
  DO(check_tree("(+ :)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


// OPT TOP TOKEN

TEST(ParserApiTokenTest, TokenOptTopFirstPresent)
{
  const char* code = "+;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_opt_top_test, "test");
  DO(check_tree("(+ :)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiTokenTest, TokenOptTopNotPresent)
{
  const char* code = ";";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_opt_top_test, "test");
  DO(check_tree("(:)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


// SKIP

TEST(ParserApiTokenTest, SkipLexError)
{
  const char* code = "$";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_test, "test");
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiTokenTest, SkipMissing)
{
  const char* code = ".";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_test, "test");
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiTokenTest, SkipFirstPresent)
{
  const char* code = "+;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_test, "test");
  DO(check_tree("(:)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiTokenTest, SkipNotFirstPresent)
{
  const char* code = "*;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_test, "test");
  DO(check_tree("(:)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiTokenTest, SkipLastPresent)
{
  const char* code = "@;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_test, "test");
  DO(check_tree("(:)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


// OPT SKIP

TEST(ParserApiTokenTest, SkipOptLexError)
{
  const char* code = "$";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_opt_test, "test");
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiTokenTest, SkipOptMissing)
{
  const char* code = ".";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_opt_test, "test");
  DO(check_tree("(:)", ast));
  DO(check_end_state(TK_DOT));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiTokenTest, SkipOptFirstPresent)
{
  const char* code = "+;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_opt_test, "test");
  DO(check_tree("(:)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}
*/
