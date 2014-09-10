#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/parserapi.h"
#include "../../src/libponyc/ast/source.h"
PONY_EXTERN_C_END

#include "util.h"
#include <gtest/gtest.h>


static bool _entered_body;
static const char* _predict_in_body;
static bool _opt_in_body;

static bool _reached_end;
static const char* _predict_at_end;
static bool _opt_at_end;
static token_id _next_token_at_end;


DEF(if_test);
  TOKEN(NULL, TK_COLON);
  PREDICT_ERROR("Foo");
  IF(TK_PLUS,
    _entered_body = true;
    _predict_in_body = parser->predicted_error;
    _opt_in_body = state.opt;
    TOKEN(NULL, TK_MINUS));
  _reached_end = true;
  _predict_at_end = parser->predicted_error;
  _opt_at_end = state.opt;
  _next_token_at_end = current_token_id(parser);
  DONE();


DEF(while_test);
  TOKEN(NULL, TK_COLON);
  PREDICT_ERROR("Foo");
  WHILE(TK_PLUS,
    _entered_body = true;
    _predict_in_body = parser->predicted_error;
    _opt_in_body = state.opt;
    TOKEN(NULL, TK_MINUS));
  _reached_end = true;
  _predict_at_end = parser->predicted_error;
  _opt_at_end = state.opt;
  _next_token_at_end = current_token_id(parser);
  DONE();


DEF(base);
  _entered_body = true;
  _predict_in_body = parser->predicted_error;
  _opt_in_body = state.opt;
  TOKEN(NULL, TK_DOT);
  DONE();

DEF(base2);
  TOKEN(NULL, TK_MINUS);
  DONE();

DEF(seq_test);
  TOKEN(NULL, TK_COLON);
  PREDICT_ERROR("Foo");
  SEQ("", base, base2);
  _reached_end = true;
  _predict_at_end = parser->predicted_error;
  _opt_at_end = state.opt;
  _next_token_at_end = current_token_id(parser);
  DONE();


class ParserApiCondTest: public testing::Test
{};


// IF

TEST(ParserApiCondTest, IfCondLexError)
{
  const char* code = ":$";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, if_test);
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_FALSE(_entered_body);
  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiCondTest, IfFalse)
{
  const char* code = ":;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, if_test);
  DO(check_tree("(: x)", ast));

  ASSERT_FALSE(_entered_body);

  ASSERT_TRUE(_reached_end);
  ASSERT_STREQ("Foo", _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, IfTrue)
{
  const char* code = ":+-;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, if_test);
  DO(check_tree("(: -)", ast));

  ASSERT_TRUE(_entered_body);
  ASSERT_EQ((void*)NULL, _predict_in_body);
  ASSERT_FALSE(_opt_in_body);

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, IfBodyLexError)
{
  const char* code = ":+$";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, if_test);
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_TRUE(_entered_body);

  ASSERT_FALSE(_reached_end);

  ast_free(ast);
  source_close(src);
}


// WHILE

TEST(ParserApiCondTest, WhileCondLexError)
{
  const char* code = ":$";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, while_test);
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_FALSE(_entered_body);
  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiCondTest, WhileTokenNone)
{
  const char* code = ":;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, while_test);
  DO(check_tree("(:)", ast));

  ASSERT_FALSE(_entered_body);

  ASSERT_TRUE(_reached_end);
  ASSERT_STREQ("Foo", _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, WhileTokenOne)
{
  const char* code = ":+-;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, while_test);
  DO(check_tree("(: -)", ast));

  ASSERT_TRUE(_entered_body);
  ASSERT_EQ((void*)NULL, _predict_in_body);
  ASSERT_FALSE(_opt_in_body);

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, WhileTokenMany)
{
  const char* code = ":+-+-+-;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, while_test);
  DO(check_tree("(: - - -)", ast));

  ASSERT_TRUE(_entered_body);
  ASSERT_EQ((void*)NULL, _predict_in_body);
  ASSERT_FALSE(_opt_in_body);

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, WhileBodyLexError)
{
  const char* code = ":+-+$";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, while_test);
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_TRUE(_entered_body);

  ASSERT_FALSE(_reached_end);

  ast_free(ast);
  source_close(src);
}


// SEQ

TEST(ParserApiCondTest, SeqNone)
{
  const char* code = ":;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, seq_test);
  DO(check_tree("(:)", ast));

  ASSERT_TRUE(_entered_body);
  ASSERT_STREQ("Foo", _predict_in_body);
  ASSERT_FALSE(_opt_in_body);

  ASSERT_TRUE(_reached_end);
  ASSERT_STREQ("Foo", _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, SeqOne)
{
  const char* code = ":.;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, seq_test);
  DO(check_tree("(: .)", ast));

  ASSERT_TRUE(_entered_body);
  ASSERT_EQ((void*)NULL, _predict_in_body); // LAST time in body
  ASSERT_FALSE(_opt_in_body);

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, SeqMany)
{
  const char* code = ":---.-..;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, seq_test);
  DO(check_tree("(: - - - . - . .)", ast));

  ASSERT_TRUE(_entered_body);
  ASSERT_EQ((void*)NULL, _predict_in_body);
  ASSERT_FALSE(_opt_in_body);

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, SeqLexError)
{
  const char* code = ":$";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, seq_test);
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_FALSE(_entered_body);  // Error immediately before body entered
  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiCondTest, SeqLexError2)
{
  const char* code = ":.$";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, seq_test);
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_TRUE(_entered_body);  // Error before body entered second time
  ASSERT_FALSE(_reached_end);

  source_close(src);
}
