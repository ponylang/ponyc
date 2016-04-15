#include <gtest/gtest.h>
#include <platform.h>
#include <stdio.h>

#include <ast/ast.h>
#include <ast/id_internal.h>
#include <ast/id.h>

#include "util.h"


static void test_id(const char* id, bool expect_pass, int spec)
{
  ast_t* node = ast_blank(TK_ID);
  ast_set_name(node, id);

  pass_opt_t opt;
  pass_opt_init(&opt);

  bool r = check_id(&opt, node, "test", spec);

  pass_opt_done(&opt);

  ast_free(node);

  if(r != expect_pass)
  {
    printf("Expected [0x%x] \"%s\" to %s but it %s\n", spec, id,
      expect_pass ? "pass" : "fail",
      r ? "passed" : "failed");
    ASSERT_TRUE(false);
  }
}


class IdTest : public testing::Test
{};


TEST_F(IdTest, LeadingUnderscore)
{
  DO(test_id("foo", true, 0));
  DO(test_id("_foo", false, 0));
  DO(test_id("foo", true, ALLOW_LEADING_UNDERSCORE));
  DO(test_id("_foo", true, ALLOW_LEADING_UNDERSCORE));
}


TEST_F(IdTest, StartUpperCase)
{
  DO(test_id("foo", false, START_UPPER));
  DO(test_id("Foo", true, START_UPPER));
  DO(test_id("_foo", false, START_UPPER));
  DO(test_id("_Foo", false, START_UPPER));
  DO(test_id("foo", false, START_UPPER | ALLOW_LEADING_UNDERSCORE));
  DO(test_id("Foo", true, START_UPPER | ALLOW_LEADING_UNDERSCORE));
  DO(test_id("_foo", false, START_UPPER | ALLOW_LEADING_UNDERSCORE));
  DO(test_id("_Foo", true, START_UPPER | ALLOW_LEADING_UNDERSCORE));
}


TEST_F(IdTest, StartLowerCase)
{
  DO(test_id("foo", true, START_LOWER));
  DO(test_id("Foo", false, START_LOWER));
  DO(test_id("_foo", false, START_LOWER));
  DO(test_id("_Foo", false, START_LOWER));
  DO(test_id("foo", true, START_LOWER | ALLOW_LEADING_UNDERSCORE));
  DO(test_id("Foo", false, START_LOWER | ALLOW_LEADING_UNDERSCORE));
  DO(test_id("_foo", true, START_LOWER | ALLOW_LEADING_UNDERSCORE));
  DO(test_id("_Foo", false, START_LOWER | ALLOW_LEADING_UNDERSCORE));
}


TEST_F(IdTest, StartAnyCase)
{
  DO(test_id("foo", true, 0));
  DO(test_id("Foo", true, 0));
  DO(test_id("_foo", false, 0));
  DO(test_id("_Foo", false, 0));
  DO(test_id("foo", true, ALLOW_LEADING_UNDERSCORE));
  DO(test_id("Foo", true, ALLOW_LEADING_UNDERSCORE));
  DO(test_id("_foo", true, ALLOW_LEADING_UNDERSCORE));
  DO(test_id("_Foo", true, ALLOW_LEADING_UNDERSCORE));
}


TEST_F(IdTest, ContainUnderscore)
{
  DO(test_id("foo_bar", false, 0));
  DO(test_id("_foo_bar", false, 0));
  DO(test_id("foo_bar", false, ALLOW_LEADING_UNDERSCORE));
  DO(test_id("_foo_bar", false, ALLOW_LEADING_UNDERSCORE));
  DO(test_id("foo_bar", true, ALLOW_UNDERSCORE));
  DO(test_id("_foo_bar", false, ALLOW_UNDERSCORE));
  DO(test_id("foo_bar", true, ALLOW_LEADING_UNDERSCORE | ALLOW_UNDERSCORE));
  DO(test_id("_foo_bar", true, ALLOW_LEADING_UNDERSCORE | ALLOW_UNDERSCORE));
}


TEST_F(IdTest, DoubleUnderscore)
{
  DO(test_id("foo__bar", false, 0));
  DO(test_id("__foobar", false, 0));
  DO(test_id("__foo__bar", false, 0));
  DO(test_id("foo__bar", false, ALLOW_UNDERSCORE));
  DO(test_id("__foobar", false, ALLOW_UNDERSCORE));
  DO(test_id("__foo__bar", false, ALLOW_UNDERSCORE));
  DO(test_id("foo__bar", false, ALLOW_LEADING_UNDERSCORE));
  DO(test_id("__foobar", false, ALLOW_LEADING_UNDERSCORE));
  DO(test_id("__foo__bar", false, ALLOW_LEADING_UNDERSCORE));
  DO(test_id("foo__bar", false, ALLOW_LEADING_UNDERSCORE | ALLOW_UNDERSCORE));
  DO(test_id("__foobar", false, ALLOW_LEADING_UNDERSCORE | ALLOW_UNDERSCORE));
  DO(test_id("__foo__bar", false,
    ALLOW_LEADING_UNDERSCORE | ALLOW_UNDERSCORE));
}


TEST_F(IdTest, TripleUnderscore)
{
  DO(test_id("foo___bar", false, 0));
  DO(test_id("___foobar", false, 0));
  DO(test_id("___foo___bar", false, 0));
  DO(test_id("foo___bar", false, ALLOW_UNDERSCORE));
  DO(test_id("___foobar", false, ALLOW_UNDERSCORE));
  DO(test_id("___foo___bar", false, ALLOW_UNDERSCORE));
  DO(test_id("foo___bar", false, ALLOW_LEADING_UNDERSCORE));
  DO(test_id("___foobar", false, ALLOW_LEADING_UNDERSCORE));
  DO(test_id("___foo___bar", false, ALLOW_LEADING_UNDERSCORE));
  DO(test_id("foo___bar", false, ALLOW_LEADING_UNDERSCORE | ALLOW_UNDERSCORE));
  DO(test_id("___foobar", false, ALLOW_LEADING_UNDERSCORE | ALLOW_UNDERSCORE));
  DO(test_id("___foo___bar", false,
    ALLOW_LEADING_UNDERSCORE | ALLOW_UNDERSCORE));
}


TEST_F(IdTest, TrailingUnderscore)
{
  DO(test_id("foo_bar", false, 0));
  DO(test_id("foobar_", false, 0));
  DO(test_id("foo_bar_", false, 0));
  DO(test_id("foobar_'", false, 0));
  DO(test_id("foo_bar", true, ALLOW_UNDERSCORE));
  DO(test_id("foobar_", false, ALLOW_UNDERSCORE));
  DO(test_id("foo_bar_", false, ALLOW_UNDERSCORE));
  DO(test_id("foobar_'", false, ALLOW_UNDERSCORE));
  DO(test_id("foo_bar", true, ALLOW_UNDERSCORE | ALLOW_TICK));
  DO(test_id("foobar_", false, ALLOW_UNDERSCORE | ALLOW_TICK));
  DO(test_id("foo_bar_", false, ALLOW_UNDERSCORE | ALLOW_TICK));
  DO(test_id("foobar_'", false, ALLOW_UNDERSCORE | ALLOW_TICK));
}


TEST_F(IdTest, Prime)
{
  DO(test_id("foo", true, 0));
  DO(test_id("fo'o", false, 0));
  DO(test_id("fo''o", false, 0));
  DO(test_id("foo'", false, 0));
  DO(test_id("foo''", false, 0));
  DO(test_id("foo'''", false, 0));
  DO(test_id("foo", true, ALLOW_TICK));
  DO(test_id("fo'o", false, ALLOW_TICK));
  DO(test_id("fo''o", false, ALLOW_TICK));
  DO(test_id("foo'", true, ALLOW_TICK));
  DO(test_id("foo''", true, ALLOW_TICK));
  DO(test_id("foo'''", true, ALLOW_TICK));
}


TEST_F(IdTest, PrivateName)
{
  ASSERT_TRUE(is_name_private("_foo"));
  ASSERT_TRUE(is_name_private("_Foo"));
  ASSERT_TRUE(is_name_private("$_foo"));
  ASSERT_TRUE(is_name_private("$_Foo"));
  ASSERT_FALSE(is_name_private("foo"));
  ASSERT_FALSE(is_name_private("Foo"));
  ASSERT_FALSE(is_name_private("$foo"));
  ASSERT_FALSE(is_name_private("$Foo"));
}


TEST_F(IdTest, TypeName)
{
  ASSERT_TRUE(is_name_type("Foo"));
  ASSERT_TRUE(is_name_type("_Foo"));
  ASSERT_TRUE(is_name_type("$Foo"));
  ASSERT_TRUE(is_name_type("$_Foo"));
  ASSERT_FALSE(is_name_type("foo"));
  ASSERT_FALSE(is_name_type("_foo"));
  ASSERT_FALSE(is_name_type("$foo"));
  ASSERT_FALSE(is_name_type("$_foo"));
}


TEST_F(IdTest, FFIType)
{
  ASSERT_TRUE(is_name_ffi("@foo"));
  ASSERT_FALSE(is_name_ffi("foo"));
}


TEST_F(IdTest, InternalTypeName)
{
  ASSERT_TRUE(is_name_internal_test("$foo"));
  ASSERT_TRUE(is_name_internal_test("$_foo"));
  ASSERT_TRUE(is_name_internal_test("$Foo"));
  ASSERT_TRUE(is_name_internal_test("$_Foo"));
  ASSERT_FALSE(is_name_internal_test("foo"));
  ASSERT_FALSE(is_name_internal_test("_foo"));
  ASSERT_FALSE(is_name_internal_test("Foo"));
  ASSERT_FALSE(is_name_internal_test("_Foo"));
}
