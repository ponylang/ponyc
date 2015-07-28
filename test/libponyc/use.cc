#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>
#include <ast/builder.h>
#include <pass/pass.h>
#include <pkg/use.h>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "scope"))
#define TEST_ERROR(src) DO(test_error(src, "scope"))


// Defined in use.c
typedef bool(*use_handler_t)(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options);

extern "C"
{
  void use_test_handler(use_handler_t handler, bool allow_alias);
}


static ast_t* received_ast;
static const char* received_locator;
static const char* received_name;
static int call_count;
static bool return_value;


static bool handler(ast_t* use, const char* locator, ast_t* name,
  pass_opt_t* options)
{
  (void)options;
  received_ast = use;
  received_locator = locator;
  call_count++;

  if(name != NULL && ast_id(name) == TK_ID)
    received_name = ast_name(name);

  return return_value;
}


class UseTest : public PassTest
{
protected:
  pass_opt_t _options;

  virtual void SetUp()
  {
    PassTest::SetUp();
    received_ast = NULL;
    received_locator = NULL;
    received_name = NULL;
    call_count = 0;
    return_value = true;
    _options.release = true;
    use_test_handler(handler, false);
  }
};


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

  use_test_handler(handler, true);
  TEST_COMPILE(src);

  ASSERT_EQ(1, call_count);
  ASSERT_STREQ("foo", received_locator);
  ASSERT_STREQ("bar", received_name);
}


TEST_F(UseTest, LegalAliasOptional)
{
  const char* src =
    "use \"test:foo\"";

  use_test_handler(handler, true);
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


TEST_F(UseTest, TrueConditionPasses)
{
  const char* src =
    "use \"test:foo\" if true";

  TEST_COMPILE(src);

  ASSERT_EQ(1, call_count);
  ASSERT_STREQ("foo", received_locator);
  ASSERT_EQ((void*)NULL, received_name);
}


TEST_F(UseTest, FalseConditionPasses)
{
  const char* src =
    "use \"test:foo\" if false";

  TEST_COMPILE(src);

  ASSERT_EQ(0, call_count);
}


TEST_F(UseTest, BadOsNameInCondition)
{
  const char* src =
    "use \"test:foo\" if wombat";

  TEST_ERROR(src);

  ASSERT_EQ(0, call_count);
}


TEST_F(UseTest, OsNameCaseSensitiveInCondition)
{
  const char* src =
    "use \"test:foo\" if WINDOWS";

  TEST_ERROR(src);

  ASSERT_EQ(0, call_count);
}


TEST_F(UseTest, AndOpInCondition)
{
  const char* src =
    "use \"test:foo\" if (linux and windows)";

  TEST_COMPILE(src);

  ASSERT_EQ(0, call_count);
}


TEST_F(UseTest, OrOpInCondition)
{
  const char* src =
    "use \"test:foo\" if (linux or windows or osx or freebsd)";

  TEST_COMPILE(src);

  ASSERT_EQ(1, call_count);
  ASSERT_STREQ("foo", received_locator);
  ASSERT_EQ((void*)NULL, received_name);
}


TEST_F(UseTest, NotOpInCondition)
{
  const char* src =
    "use \"test:foo\" if (not false)";

  TEST_COMPILE(src);

  ASSERT_EQ(1, call_count);
  ASSERT_STREQ("foo", received_locator);
  ASSERT_EQ((void*)NULL, received_name);
}
