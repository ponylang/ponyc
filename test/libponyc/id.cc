#include <gtest/gtest.h>
#include <platform.h>
#include <stdio.h>

#include <ast/ast.h>
#include <ast/id.h>

#include "util.h"

// Defined in id.c
#define START_UPPER               0x01
#define START_LOWER               0x02
#define ALLOW_LEADING_UNDERSCORE  0x04
#define ALLOW_UNDERSCORE          0x08
#define ALLOW_TICK                0x10

PONY_EXTERN_C_BEGIN
bool check_id(ast_t* id_node, const char* desc, int spec);
PONY_EXTERN_C_END


static void test_id(const char* id, bool expect_pass, int spec)
{
  ast_t* node = ast_blank(TK_ID);
  ast_set_name(node, id);

  bool r = check_id(node, "test", spec);

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
