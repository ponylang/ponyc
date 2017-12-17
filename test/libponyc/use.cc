#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <pass/pass.h>
#include <pkg/use.h>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "scope"))
#define TEST_ERROR(src) DO(test_error(src, "scope"))


static bool handler(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options);


class UseTest : public PassTest
{
public:
  ast_t* received_ast;
  const char* received_locator;
  const char* received_name;
  int call_count;
  bool return_value;

  virtual void SetUp()
  {
    PassTest::SetUp();
    received_ast = NULL;
    received_locator = NULL;
    received_name = NULL;
    call_count = 0;
    return_value = true;
    opt.release = true;
    opt.data = this;
    use_test_handler(handler, false, true);
  }
};


static bool handler(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options)
{
  UseTest* pass = (UseTest*)options->data;
  pass->received_ast = use;
  pass->received_locator = locator;
  pass->call_count++;

  if(name != NULL && ast_id(name) == TK_ID)
    pass->received_name = ast_name(name);

  return pass->return_value;
}


TEST_F(UseTest, Basic)
{
  const char* src =
    "use \"test:foo\"";

  TEST_COMPILE(src);

  ASSERT_EQ(1, call_count);
  ASSERT_STREQ("foo", received_locator);
  ASSERT_EQ((void*)NULL, received_name);
}


TEST_F(UseTest, Alias)
{
  const char* src =
    "use bar = \"test:foo\"";

  use_test_handler(handler, true, true);
  TEST_COMPILE(src);

  ASSERT_EQ(1, call_count);
  ASSERT_STREQ("foo", received_locator);
  ASSERT_STREQ("bar", received_name);
}


TEST_F(UseTest, LegalAliasOptional)
{
  const char* src =
    "use \"test:foo\"";

  use_test_handler(handler, true, true);
  TEST_COMPILE(src);

  ASSERT_EQ(1, call_count);
  ASSERT_STREQ("foo", received_locator);
  ASSERT_EQ((void*)NULL, received_name);
}


TEST_F(UseTest, IllegalAlias)
{
  const char* src =
    "use bar = \"test:foo\"";

  TEST_ERROR(src);

  ASSERT_EQ(0, call_count);
}


TEST_F(UseTest, SchemeNotFound)
{
  const char* src =
    "use \"notest:foo\"";

  TEST_ERROR(src);

  ASSERT_EQ(0, call_count);
}


TEST_F(UseTest, SchemeCaseSensitive)
{
  const char* src =
    "use \"TeSt:foo\"";

  TEST_ERROR(src);

  ASSERT_EQ(0, call_count);
}


TEST_F(UseTest, FalseReturnPassedThrough)
{
  const char* src =
    "use \"test:foo\"";

  return_value = false;
  TEST_ERROR(src);

  ASSERT_EQ(1, call_count);
}


TEST_F(UseTest, IllegalCondition)
{
  const char* src =
    "use \"test:foo\" if debug";

  use_test_handler(handler, true, false);
  TEST_ERROR(src);

  ASSERT_EQ(0, call_count);
}


TEST_F(UseTest, TrueConditionPasses)
{
  const char* src =
    "use \"test:foo\" if (debug or not debug)";

  TEST_COMPILE(src);

  ASSERT_EQ(1, call_count);
  ASSERT_STREQ("foo", received_locator);
  ASSERT_EQ((void*)NULL, received_name);
}


TEST_F(UseTest, FalseConditionFails)
{
  const char* src =
    "use \"test:foo\" if debug\n"
    "use \"test:foo\" if not debug";

  TEST_COMPILE(src);

  ASSERT_EQ(1, call_count);
}
