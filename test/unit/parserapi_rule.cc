#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/parserapi.h"
#include "../../src/libponyc/ast/source.h"
PONY_EXTERN_C_END

#include "util.h"
#include <gtest/gtest.h>


static bool _reached_end;
static bool _opt_at_end;
static bool _top_at_end;
static token_id _next_token_at_end;


DEF(base_dot);
  TOKEN(NULL, TK_DOT);
  DONE();

DEF(base_bang);
  TOKEN(NULL, TK_BANG);
  DONE();

DEF(base_plus);
  TOKEN(NULL, TK_PLUS);
  TOKEN(NULL, TK_PLUS);
  DONE();

DEF(rule_test);
  TOKEN(NULL, TK_COLON);
  RULE("test", base_dot, base_bang, base_plus);
  _reached_end = true;
  _opt_at_end = state.opt;
  _top_at_end = state.top;
  _next_token_at_end = current_token_id(parser);
  DONE();

DEF(rule_opt_test);
  TOKEN(NULL, TK_COLON);
  OPT RULE("test", base_dot, base_bang, base_plus);
  _reached_end = true;
  _opt_at_end = state.opt;
  _top_at_end = state.top;
  _next_token_at_end = current_token_id(parser);
  DONE();

DEF(rule_top_test);
  TOKEN(NULL, TK_COLON);
  TOKEN(NULL, TK_COLON);
  TOP RULE("test", base_dot, base_bang, base_plus);
  _reached_end = true;
  _opt_at_end = state.opt;
  _top_at_end = state.top;
  _next_token_at_end = current_token_id(parser);
  DONE();

DEF(rule_opt_top_test);
  TOKEN(NULL, TK_COLON);
  TOKEN(NULL, TK_COLON);
  OPT TOP RULE("test", base_dot, base_bang, base_plus);
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


class ParserApiRuleTest: public testing::Test
{};


// RULE

TEST(ParserApiRuleTest, RuleLexError)
{
  const char* code = ":$";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_test, "test");
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiRuleTest, RuleMissing)
{
  const char* code = ":-";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_test, "test");
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiRuleTest, RuleFirstPresent)
{
  const char* code = ":.;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_test, "test");
  DO(check_tree("(: .)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiRuleTest, RuleNotFirstPresent)
{
  const char* code = ":!;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_test, "test");
  DO(check_tree("(: !)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiRuleTest, RuleLastPresent)
{
  const char* code = ":++;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_test, "test");
  DO(check_tree("(: (+ +))", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


// OPT RULE

TEST(ParserApiRuleTest, RuleOptLexError)
{
  const char* code = ":$";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_opt_test, "test");
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_FALSE(_reached_end);

  source_close(src);
}


TEST(ParserApiRuleTest, RuleOptMissing)
{
  const char* code = ":;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_opt_test, "test");
  DO(check_tree("(: x)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiRuleTest, RuleOptFirstPresent)
{
  const char* code = ":.;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_opt_test, "test");
  DO(check_tree("(: .)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


// RULE TOP

TEST(ParserApiRuleTest, RuleTopLastPresent)
{
  const char* code = "::++;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_top_test, "test");
  DO(check_tree("(+ (: :) +)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


// RULE OPT TOP

TEST(ParserApiRuleTest, RuleOptTopLastPresent)
{
  const char* code = "::++;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_opt_top_test, "test");
  DO(check_tree("(+ (: :) +)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}


TEST(ParserApiRuleTest, RuleOptTopNotPresent)
{
  const char* code = "::;";

  source_t* src = source_open_string(code);
  _reached_end = false;

  ast_t* ast = parse(src, rule_opt_top_test, "test");
  DO(check_tree("(: :)", ast));
  DO(check_end_state(TK_SEMI));

  ast_free(ast);
  source_close(src);
}
