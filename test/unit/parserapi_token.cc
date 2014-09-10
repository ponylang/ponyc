#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/parserapi.h"
#include "../../src/libponyc/ast/source.h"
#include "../../src/libponyc/ast/error.h"
PONY_EXTERN_C_END

#include "util.h"
#include <gtest/gtest.h>


static bool _reached_end;
static const char* _predict_at_end;
static bool _opt_at_end;
static token_id _next_token_at_end;


DEF(token_test);
  AST_NODE(TK_COLON);
  PREDICT_ERROR("Foo");
  TOKEN(NULL, TK_PLUS, TK_MULTIPLY, TK_BANG);
  _reached_end = true;
  _predict_at_end = parser->predicted_error;
  _opt_at_end = state.opt;
  _next_token_at_end = current_token_id(parser);
  DONE();


DEF(token_opt_test);
  AST_NODE(TK_COLON);
  PREDICT_ERROR("Foo");
  OPT TOKEN(NULL, TK_PLUS, TK_MULTIPLY, TK_BANG);
  _reached_end = true;
  _predict_at_end = parser->predicted_error;
  _opt_at_end = state.opt;
  _next_token_at_end = current_token_id(parser);
  DONE();


DEF(skip_test);
  AST_NODE(TK_COLON);
  PREDICT_ERROR("Foo");
  SKIP(NULL, TK_PLUS, TK_MULTIPLY, TK_BANG);
  _reached_end = true;
  _predict_at_end = parser->predicted_error;
  _opt_at_end = state.opt;
  _next_token_at_end = current_token_id(parser);
  DONE();


DEF(skip_opt_test);
  AST_NODE(TK_COLON);
  PREDICT_ERROR("Foo");
  OPT SKIP(NULL, TK_PLUS, TK_MULTIPLY, TK_BANG);
  _reached_end = true;
  _predict_at_end = parser->predicted_error;
  _opt_at_end = state.opt;
  _next_token_at_end = current_token_id(parser);
  DONE();


class ParserApiTokenTest: public testing::Test
{};


// TOKEN

TEST(ParserApiTokenTest, TokenLexError)
{
  const char* code = "$";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_test);
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiTokenTest, TokenMissing)
{
  const char* code = ".";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_test);
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiTokenTest, TokenFirstPresent)
{
  const char* code = "+;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_test);
  DO(check_tree("(: +)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiTokenTest, TokenNotFirstPresent)
{
  const char* code = "*;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_test);
  DO(check_tree("(: *)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiTokenTest, TokenLastPresent)
{
  const char* code = "!;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_test);
  DO(check_tree("(: !)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


// OPT TOKEN

TEST(ParserApiTokenTest, TokenOptLexError)
{
  const char* code = "$";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_opt_test);
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiTokenTest, TokenOptMissing)
{
  const char* code = ".";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_opt_test);
  DO(check_tree("(: x)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_STREQ("Foo", _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_DOT, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiTokenTest, TokenOptFirstPresent)
{
  const char* code = "+;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, token_opt_test);
  DO(check_tree("(: +)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


// SKIP

TEST(ParserApiTokenTest, SkipLexError)
{
  const char* code = "$";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_test);
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiTokenTest, SkipMissing)
{
  const char* code = ".";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_test);
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiTokenTest, SkipFirstPresent)
{
  const char* code = "+;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_test);
  DO(check_tree("(:)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiTokenTest, SkipNotFirstPresent)
{
  const char* code = "*;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_test);
  DO(check_tree("(:)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiTokenTest, SkipLastPresent)
{
  const char* code = "!;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_test);
  DO(check_tree("(:)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


// OPT SKIP

TEST(ParserApiTokenTest, SkipOptLexError)
{
  const char* code = "$";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_opt_test);
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiTokenTest, SkipOptMissing)
{
  const char* code = ".";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_opt_test);
  DO(check_tree("(:)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_STREQ("Foo", _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_DOT, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiTokenTest, SkipOptFirstPresent)
{
  const char* code = "+;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, skip_opt_test);
  DO(check_tree("(:)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}
