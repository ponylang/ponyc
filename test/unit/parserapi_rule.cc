#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/parserapi.h"
#include "../../src/libponyc/ast/source.h"
PONY_EXTERN_C_END

#include "util.h"
#include <gtest/gtest.h>


static bool _reached_end;
static const char* _predict_at_end;
static bool _opt_at_end;
static token_id _next_token_at_end;


DEF(base_dot);
  TOKEN(NULL, TK_DOT);
  DONE();

DEF(base_bang);
  TOKEN(NULL, TK_BANG);
  DONE();

DEF(base_plus);
  TOKEN(NULL, TK_PLUS);
  DONE();

DEF(rule_test);
  TOKEN(NULL, TK_COLON);
  PREDICT_ERROR("Foo");
  RULE(NULL, base_dot, base_bang, base_plus);
  _reached_end = true;
  _predict_at_end = parser->predicted_error;
  _opt_at_end = state.opt;
  _next_token_at_end = current_token_id(parser);
  DONE();

DEF(rule_opt_test);
  TOKEN(NULL, TK_COLON);
  PREDICT_ERROR("Foo");
  OPT RULE(NULL, base_dot, base_bang, base_plus);
  _reached_end = true;
  _predict_at_end = parser->predicted_error;
  _opt_at_end = state.opt;
  _next_token_at_end = current_token_id(parser);
  DONE();


class ParserApiRuleTest: public testing::Test
{};


// RULE

TEST(ParserApiRuleTest, RuleLexError)
{
  const char* code = ":$";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_test);
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiRuleTest, RuleMissing)
{
  const char* code = ":-";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_test);
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiRuleTest, RuleFirstPresent)
{
  const char* code = ":.;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_test);
  DO(check_tree("(: .)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiRuleTest, RuleNotFirstPresent)
{
  const char* code = ":!;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_test);
  DO(check_tree("(: !)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiRuleTest, RuleLastPresent)
{
  const char* code = ":+;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_test);
  DO(check_tree("(: +)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


// OPT RULE

TEST(ParserApiRuleTest, RuleOptLexError)
{
  const char* code = ":$";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_opt_test);
  ASSERT_EQ((void*)NULL, ast);

  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiRuleTest, RuleOptMissing)
{
  const char* code = ":;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_opt_test);
  DO(check_tree("(: x)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_STREQ("Foo", _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiRuleTest, RuleOptFirstPresent)
{
  const char* code = ":.;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_opt_test);
  DO(check_tree("(: .)", ast));

  ASSERT_TRUE(_reached_end);
  ASSERT_EQ((void*)NULL, _predict_at_end);
  ASSERT_FALSE(_opt_at_end);
  ASSERT_EQ(TK_SEMI, _next_token_at_end);

  ast_free(ast);
  source_close(src);
}
