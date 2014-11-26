#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/builder.h>
#include <pass/pass.h>
#include <pkg/use.h>

#include "util.h"


static ast_t* received_ast;
static const char* received_locator;
static const char* received_name;
static pass_opt_t* received_options;
static int call_a_count;
static int call_b_count;
static bool return_value;


static bool handler_a(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options)
{
  received_ast = use;
  received_locator = locator;
  received_options = options;
  call_a_count++;

  if(name != NULL && ast_id(name) == TK_ID)
    received_name = ast_name(name);

  return return_value;
}


static bool handler_b(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options)
{
  received_ast = use;
  received_locator = locator;
  received_options = options;
  call_b_count++;

  if(name != NULL && ast_id(name) == TK_ID)
    received_name = ast_name(name);

  return return_value;
}


class UseTest : public testing::Test
{
protected:
  pass_opt_t _options;

  virtual void SetUp()
  {
    received_ast = NULL;
    received_locator = NULL;
    received_name = NULL;
    received_options = NULL;
    call_a_count = 0;
    call_b_count = 0;
    return_value = true;
    _options.release = true;
    use_clear_handlers();
  }

  void test_use(const char* before, const char* after, bool expect)
  {
    free_errors();

    // Build the before description into an AST
    builder_t* builder;
    ast_t* actual_ast;
    DO(build_ast_from_string(before, &actual_ast, &builder));
    ASSERT_NE((void*)NULL, actual_ast);

    ASSERT_EQ(expect, use_command(actual_ast, &_options));

    if(call_a_count > 0 || call_b_count > 0)
    {
      ASSERT_EQ(actual_ast, received_ast);
      ASSERT_EQ(&_options, received_options);
    }

    DO(check_tree(after, actual_ast));

    builder_free(builder);
  }
};


TEST_F(UseTest, Basic)
{
  const char* before =
    "(use x \"test:foo\" x)";

  use_register_handler("test:", false, handler_a);

  DO(test_use(before, before, true));

  ASSERT_EQ(1, call_a_count);
  ASSERT_EQ(0, call_b_count);
  ASSERT_STREQ("foo", received_locator);
  ASSERT_EQ((void*)NULL, received_name);
}


TEST_F(UseTest, Alias)
{
  const char* before =
    "(use (id bar) \"test:foo\" x)";

  use_register_handler("test:", true, handler_a);

  DO(test_use(before, before, true));

  ASSERT_EQ(1, call_a_count);
  ASSERT_EQ(0, call_b_count);
  ASSERT_STREQ("foo", received_locator);
  ASSERT_STREQ("bar", received_name);
}


TEST_F(UseTest, LegalAliasOptional)
{
  const char* before =
    "(use x \"test:foo\" x)";

  use_register_handler("test:", true, handler_a);

  DO(test_use(before, before, true));

  ASSERT_EQ(1, call_a_count);
  ASSERT_EQ(0, call_b_count);
  ASSERT_STREQ("foo", received_locator);
  ASSERT_EQ((void*)NULL, received_name);
}


TEST_F(UseTest, IllegalAlias)
{
  const char* before =
    "(use (id bar) \"test:foo\" x)";

  use_register_handler("test:", false, handler_a);

  DO(test_use(before, before, false));

  ASSERT_EQ(0, call_a_count);
  ASSERT_EQ(0, call_b_count);
}


TEST_F(UseTest, SchemeNotFound)
{
  const char* before =
    "(use x \"test:foo\" x)";

  use_register_handler("notest:", false, handler_a);

  DO(test_use(before, before, false));

  ASSERT_EQ(0, call_a_count);
  ASSERT_EQ(0, call_b_count);
}


TEST_F(UseTest, SchemeCaseSensitive)
{
  const char* before =
    "(use x \"test:foo\" x)";

  use_register_handler("TeSt:", false, handler_a);

  DO(test_use(before, before, false));

  ASSERT_EQ(0, call_a_count);
  ASSERT_EQ(0, call_b_count);
}


TEST_F(UseTest, FalseReturnPassedThrough)
{
  const char* before =
    "(use x \"test:foo\" x)";

  use_register_handler("test:", false, handler_a);
  return_value = false;

  DO(test_use(before, before, false));

  ASSERT_EQ(1, call_a_count);
  ASSERT_EQ(0, call_b_count);
}


TEST_F(UseTest, MultipleSchemesMatchesFirst)
{
  const char* before =
    "(use x \"test:foo\" x)";

  use_register_handler("test:", false, handler_a);
  use_register_handler("bar:", false, handler_b);

  DO(test_use(before, before, true));

  ASSERT_EQ(1, call_a_count);
  ASSERT_EQ(0, call_b_count);
}


TEST_F(UseTest, MultipleSchemesMatchesNotFirst)
{
  const char* before =
    "(use x \"bar:foo\" x)";

  use_register_handler("test:", false, handler_a);
  use_register_handler("bar:", false, handler_b);

  DO(test_use(before, before, true));

  ASSERT_EQ(0, call_a_count);
  ASSERT_EQ(1, call_b_count);
}


TEST_F(UseTest, DefaultSchemeIsFirst)
{
  const char* before =
    "(use x \"foo\" x)";

  use_register_handler("test:", false, handler_a);
  use_register_handler("bar:", false, handler_b);

  DO(test_use(before, before, true));

  ASSERT_EQ(1, call_a_count);
  ASSERT_EQ(0, call_b_count);
}


TEST_F(UseTest, NoSchemes)
{
  const char* before =
    "(use x \"foo\" x)";

  DO(test_use(before, before, false));

  ASSERT_EQ(0, call_a_count);
  ASSERT_EQ(0, call_b_count);
}


TEST_F(UseTest, TrueConditionPasses)
{
  const char* before =
    "(use x \"test:foo\" (reference (id debug)))";

  const char* after =
    "(use x \"test:foo\" x)";

  use_register_handler("test:", false, handler_a);
  _options.release = false;

  DO(test_use(before, after, true));

  ASSERT_EQ(1, call_a_count);
  ASSERT_EQ(0, call_b_count);
  ASSERT_STREQ("foo", received_locator);
  ASSERT_EQ((void*)NULL, received_name);
}


TEST_F(UseTest, FalseConditionPasses)
{
  const char* before =
    "(use x \"test:foo\" (reference (id debug)))";

  const char* after =
    "(x)";

  use_register_handler("test:", false, handler_a);
  _options.release = true;

  DO(test_use(before, after, true));

  ASSERT_EQ(0, call_a_count);
  ASSERT_EQ(0, call_b_count);
}


TEST_F(UseTest, BadOsNameInCondition)
{
  const char* before =
    "(use x \"test:foo\" (reference (id bar)))";

  use_register_handler("test:", false, handler_a);

  DO(test_use(before, before, false));

  ASSERT_EQ(0, call_a_count);
  ASSERT_EQ(0, call_b_count);
}


TEST_F(UseTest, OsNameCaseSensitiveInCondition)
{
  const char* before =
    "(use x \"test:foo\" (reference (id WINDOWS)))";

  use_register_handler("test:", false, handler_a);

  DO(test_use(before, before, false));

  ASSERT_EQ(0, call_a_count);
  ASSERT_EQ(0, call_b_count);
}


TEST_F(UseTest, AndOpInCondition)
{
  const char* before =
    "(use x \"test:foo\"\n"
    "  (tuple (seq (call\n"
    "    (positionalargs (reference (id osx)))\n"
    "    x\n"
    "    (. (reference (id linux)) (id and_))))))";

  const char* after =
    "(x)";

  use_register_handler("test:", false, handler_a);

  DO(test_use(before, after, true));

  ASSERT_EQ(0, call_a_count);
  ASSERT_EQ(0, call_b_count);
}


TEST_F(UseTest, NotOpInCondition)
{
  const char* before =
    "(use x \"test:foo\"\n"
    "  (tuple (seq (call\n"
    "    x x (. (reference (id debug)) (id not_))))))";

  const char* after =
    "(use x \"test:foo\" x)";

  use_register_handler("test:", false, handler_a);
  _options.release = true;

  DO(test_use(before, after, true));

  ASSERT_EQ(1, call_a_count);
  ASSERT_EQ(0, call_b_count);
}
