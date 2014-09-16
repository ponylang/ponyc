#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/parserapi.h"
#include "../../src/libponyc/ast/source.h"
PONY_EXTERN_C_END

#include "util.h"
#include <gtest/gtest.h>


static bool _entered_body;
static bool _opt_in_body;
static bool _top_in_body;

static bool _reached_end;
static bool _opt_at_end;
static bool _top_at_end;
static token_id _next_token_at_end;


DEF(if_test);
  TOKEN(NULL, TK_COLON);
  IF(TK_PLUS,
    _entered_body = true;
    _opt_in_body = state.opt;
    _top_in_body = state.top;
    TOKEN(NULL, TK_MINUS));
  _reached_end = true;
  _opt_at_end = state.opt;
  _top_at_end = state.top;
  _next_token_at_end = current_token_id(parser);
  DONE();


DEF(while_test);
  TOKEN(NULL, TK_COLON);
  WHILE(TK_PLUS,
    _entered_body = true;
    _opt_in_body = state.opt;
    _top_in_body = state.top;
    TOKEN(NULL, TK_MINUS));
  _reached_end = true;
  _opt_at_end = state.opt;
  _top_at_end = state.top;
  _next_token_at_end = current_token_id(parser);
  DONE();


DEF(base);
  _entered_body = true;
  _opt_in_body = state.opt;
  _top_in_body = state.top;
  TOKEN(NULL, TK_DOT);
  DONE();

DEF(base2);
  TOKEN(NULL, TK_MINUS);
  DONE();

DEF(base3);
  TOKEN(NULL, TK_PLUS);
  TOKEN(NULL, TK_PLUS);
  DONE();

DEF(seq_test);
  TOKEN(NULL, TK_COLON);
  SEQ("test", base, base2);
  _reached_end = true;
  _opt_at_end = state.opt;
  _top_at_end = state.top;
  _next_token_at_end = current_token_id(parser);
  DONE();

DEF(seq_top_test);
  TOKEN(NULL, TK_COLON);
  TOKEN(NULL, TK_COLON);
  TOP SEQ("test", base, base3);
  _reached_end = true;
  _opt_at_end = state.opt;
  _top_at_end = state.top;
  _next_token_at_end = current_token_id(parser);
  DONE();

  
static void check_body_state()
{
  ASSERT_TRUE(_entered_body);
  ASSERT_FALSE(_opt_in_body);
  ASSERT_FALSE(_top_in_body);
}


static void check_end_state(token_id expect_token)
{
  ASSERT_TRUE(_reached_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_FALSE(_top_at_end);
  ASSERT_EQ(expect_token, _next_token_at_end);
}


class ParserApiCondTest: public testing::Test
{};


// IF

TEST(ParserApiCondTest, IfCondLexError)
{
  const char* code = ":$";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, if_test, "test");
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

  ast_t* ast = parse(src, if_test, "test");
  DO(check_tree("(: x)", ast));
  ASSERT_FALSE(_entered_body);
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, IfTrue)
{
  const char* code = ":+-;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, if_test, "test");
  DO(check_tree("(: -)", ast));
  DO(check_body_state());
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, IfBodyLexError)
{
  const char* code = ":+$";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, if_test, "test");
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

  ast_t* ast = parse(src, while_test, "test");
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

  ast_t* ast = parse(src, while_test, "test");
  DO(check_tree("(:)", ast));
  ASSERT_FALSE(_entered_body);
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, WhileTokenOne)
{
  const char* code = ":+-;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, while_test, "test");
  DO(check_tree("(: -)", ast));
  DO(check_body_state());
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, WhileTokenMany)
{
  const char* code = ":+-+-+-;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, while_test, "test");
  DO(check_tree("(: - - -)", ast));
  DO(check_body_state());
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, WhileBodyLexError)
{
  const char* code = ":+-+$";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, while_test, "test");
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

  ast_t* ast = parse(src, seq_test, "test");
  DO(check_tree("(:)", ast));
  DO(check_body_state());
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, SeqOne)
{
  const char* code = ":.;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, seq_test, "test");
  DO(check_tree("(: .)", ast));
  DO(check_body_state()); // LAST time in body
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, SeqMany)
{
  const char* code = ":---.-..;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, seq_test, "test");
  DO(check_tree("(: - - - . - . .)", ast));
  DO(check_body_state());
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, SeqLexError)
{
  const char* code = ":$";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, seq_test, "test");
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

  ast_t* ast = parse(src, seq_test, "test");
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_TRUE(_entered_body);  // Error before body entered second time
  ASSERT_FALSE(_reached_end);

  source_close(src);
}


// TOP SEQ

TEST(ParserApiCondTest, SeqTopNone)
{
  const char* code = "::;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, seq_top_test, "test");
  DO(check_tree("(: :)", ast));
  DO(check_body_state());
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, SeqTopOne)
{
  const char* code = "::++;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, seq_top_test, "test");
  DO(check_tree("(+ (: :) +)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiCondTest, SeqTopMulti)
{
  const char* code = "::++..;";

  source_t* src = source_open_string(code);

  _entered_body = false;
  _reached_end = false;

  ast_t* ast = parse(src, seq_top_test, "test");
  DO(check_tree("(. (. (+ (: :) +)))", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}
