#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/parserapi.h"
#include "../../src/libponyc/ast/source.h"
PONY_EXTERN_C_END

#include <gtest/gtest.h>


DEF(base_dot);
  TOKEN(NULL, TK_DOT);
  DONE();

DEF(base_bang);
  TOKEN(NULL, TK_BANG);
  DONE();

DEF(base_colon);
  TOKEN(NULL, TK_COLON);
  DONE();

DEF(base_semi);
  TOKEN(NULL, TK_SEMI);
  DONE();

DEF(base_compound);
  TOKEN(NULL, TK_PLUS);
  TOKEN(NULL, TK_PLUS);
  DONE();

DEF(rule_dot);
  AST_NODE(TK_INT);
  RULE("", base_dot);
  DONE();

DEF(rule_multi);
  AST_NODE(TK_INT);
  RULE("", base_bang, base_colon, base_semi, base_compound);
  DONE();

DEF(optional_dot);
  AST_NODE(TK_INT);
  OPT RULE("", base_dot);
  DONE();

DEF(optional_multi);
  AST_NODE(TK_INT);
  OPT RULE("", base_bang, base_colon, base_semi, base_compound);
  DONE();


class ParserApiRuleTest: public testing::Test
{};


// RULE

TEST(ParserApiRuleTest, Rule)
{
  const char* code = ".";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, rule_dot);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_DOT, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiRuleTest, RuleMissing)
{
  const char* code = ":";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, rule_dot);
  ASSERT_EQ((void*)NULL, ast);

  source_close(src);
}


TEST(ParserApiRuleTest, RuleMultiFirst)
{
  const char* code = "!";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, rule_multi);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_BANG, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiRuleTest, RuleMultiNotFirst)
{
  const char* code = ":";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, rule_multi);
  ASSERT_NE((void*)NULL, ast);
  ASSERT_EQ(TK_INT, ast_id(ast));
  ASSERT_NE((void*)NULL, ast_child(ast));
  ASSERT_EQ(TK_COLON, ast_id(ast_child(ast)));
  ast_free(ast);

  source_close(src);
}


TEST(ParserApiRuleTest, RuleMultiMissing)
{
  const char* code = "3";

  source_t* src = source_open_string(code);

  ast_t* ast = parse(src, rule_multi);
  ASSERT_EQ((void*)NULL, ast);

  source_close(src);
}


TEST(ParserApiRuleTest, RuleLexError)
{
  const char* code = "$";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, rule_multi);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}


TEST(ParserApiRuleTest, RuleParseError)
{
  const char* code = "+ -";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, rule_multi);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}


// OPT RULE

TEST(ParserApiRuleTest, Optional)
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


TEST(ParserApiRuleTest, OptionalNotPresent)
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


TEST(ParserApiRuleTest, OptionalMultiFirst)
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


TEST(ParserApiRuleTest, OptionalMultiNotFirst)
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


TEST(ParserApiRuleTest, OptionalMultiNotPresent)
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


TEST(ParserApiRuleTest, OptionalLexError)
{
  const char* code = "$";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, optional_multi);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}


TEST(ParserApiRuleTest, OptionalParseError)
{
  const char* code = "+ -";

  source_t* src = source_open_string(code);
  free_errors();

  ast_t* ast = parse(src, optional_multi);
  ASSERT_EQ((void*)NULL, ast);
  ASSERT_EQ(1, get_error_count());

  source_close(src);
}
