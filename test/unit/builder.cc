#include "../../src/libponyc/platform/platform.h"

PONY_EXTERN_C_BEGIN
#include "../../src/libponyc/ast/ast.h"
#include "../../src/libponyc/ast/builder.h"
#include "../../src/libponyc/ast/source.h"
#include "../../src/libponyc/ast/token.h"
PONY_EXTERN_C_END

#include <gtest/gtest.h>


class BuilderTest: public testing::Test
{};


TEST(BuilderTest, EqualNoScope)
{
  source_t* src = source_open_string("Test");
  token_t* t = token_new(TK_INT, src);

  ast_t* expected = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 3);
  ast_add(expected, ast_new(t, TK_INT));

  token_set_int(t, 4);
  ast_add(expected, ast_new(t, TK_INT));

  token_set_int(t, 5);
  ast_settype(expected, ast_new(t, TK_INT));

  ast_t* actual = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 3);
  ast_add(actual, ast_new(t, TK_INT));

  token_set_int(t, 4);
  ast_add(actual, ast_new(t, TK_INT));

  token_set_int(t, 5);
  ast_settype(actual, ast_new(t, TK_INT));

  ASSERT_TRUE(build_compare_asts(expected, actual));

  ast_free(expected);
  ast_free(actual);
  token_free(t);
  source_close(src);
}


TEST(BuilderTest, DifferingIds)
{
  source_t* src = source_open_string("Test");

  ast_t* expected = ast_token(token_new(TK_PLUS, src));
  ast_t* actual = ast_token(token_new(TK_MINUS, src));
  free_errors();

  ASSERT_FALSE(build_compare_asts(expected, actual));
  ASSERT_EQ(1, get_error_count());

  ast_free(expected);
  ast_free(actual);
  source_close(src);
}


TEST(BuilderTest, DifferingIntValue)
{
  source_t* src = source_open_string("Test");
  token_t* t = token_new(TK_INT, src);
  token_t* t2 = token_new(TK_INT, src);

  ast_t* expected = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 3);
  ast_add(expected, ast_new(t, TK_INT));

  ast_t* actual = ast_token(token_new(TK_PLUS, src));

  token_set_int(t2, 4);
  ast_add(actual, ast_new(t2, TK_INT));

  free_errors();

  ASSERT_FALSE(build_compare_asts(expected, actual));
  ASSERT_EQ(1, get_error_count());

  ast_free(expected);
  ast_free(actual);
  source_close(src);
}


TEST(BuilderTest, DifferingNames)
{
  source_t* src = source_open_string("Test");

  token_t* t = token_new(TK_ID, src);
  token_set_string(t, "foo");
  ast_t* expected = ast_token(t);

  token_t* t2 = token_new(TK_ID, src);
  token_set_string(t2, "bar");
  ast_t* actual = ast_token(t2);

  free_errors();

  ASSERT_FALSE(build_compare_asts(expected, actual));
  ASSERT_EQ(1, get_error_count());

  ast_free(expected);
  ast_free(actual);
  source_close(src);
}


TEST(BuilderTest, HygenicNameMatch)
{
  source_t* src = source_open_string("Test");

  token_t* t = token_new(TK_ID, src);
  token_set_string(t, "hygid");
  ast_t* expected = ast_token(t);

  token_t* t2 = token_new(TK_ID, src);
  token_set_string(t2, "$37");
  ast_t* actual = ast_token(t2);

  ASSERT_TRUE(build_compare_asts(expected, actual));

  ast_free(expected);
  ast_free(actual);
  source_close(src);
}


TEST(BuilderTest, MissingChild)
{
  source_t* src = source_open_string("Test");
  token_t* t = token_new(TK_INT, src);

  ast_t* expected = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 3);
  ast_add(expected, ast_new(t, TK_INT));

  ast_t* actual = ast_token(token_new(TK_PLUS, src));

  free_errors();

  ASSERT_FALSE(build_compare_asts(expected, actual));
  ASSERT_EQ(1, get_error_count());

  ast_free(expected);
  ast_free(actual);
  token_free(t);
  source_close(src);
}


TEST(BuilderTest, UnexpectedChild)
{
  source_t* src = source_open_string("Test");
  token_t* t = token_new(TK_INT, src);

  ast_t* expected = ast_token(token_new(TK_PLUS, src));

  ast_t* actual = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 3);
  ast_add(actual, ast_new(t, TK_INT));

  free_errors();

  ASSERT_FALSE(build_compare_asts(expected, actual));
  ASSERT_EQ(1, get_error_count());

  ast_free(expected);
  ast_free(actual);
  source_close(src);
}


TEST(BuilderTest, DifferingChild)
{
  source_t* src = source_open_string("Test");
  token_t* t = token_new(TK_INT, src);

  ast_t* expected = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 3);
  ast_add(expected, ast_new(t, TK_INT));

  ast_t* actual = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 4);
  ast_add(actual, ast_new(t, TK_INT));

  free_errors();

  ASSERT_FALSE(build_compare_asts(expected, actual));
  ASSERT_EQ(1, get_error_count());

  ast_free(expected);
  ast_free(actual);
  token_free(t);
  source_close(src);
}


TEST(BuilderTest, MissingSibling)
{
  source_t* src = source_open_string("Test");
  token_t* t = token_new(TK_INT, src);

  ast_t* expected = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 4);
  ast_add(expected, ast_new(t, TK_INT));

  token_set_int(t, 3);
  ast_add(expected, ast_new(t, TK_INT));

  ast_t* actual = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 3);
  ast_add(actual, ast_new(t, TK_INT));

  free_errors();

  ASSERT_FALSE(build_compare_asts(expected, actual));
  ASSERT_EQ(1, get_error_count());

  ast_free(expected);
  ast_free(actual);
  token_free(t);
  source_close(src);
}


TEST(BuilderTest, UnexpectedSibling)
{
  source_t* src = source_open_string("Test");
  token_t* t = token_new(TK_INT, src);

  ast_t* expected = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 3);
  ast_add(expected, ast_new(t, TK_INT));

  ast_t* actual = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 4);
  ast_add(actual, ast_new(t, TK_INT));

  token_set_int(t, 3);
  ast_add(actual, ast_new(t, TK_INT));

  free_errors();

  ASSERT_FALSE(build_compare_asts(expected, actual));
  ASSERT_EQ(1, get_error_count());

  ast_free(expected);
  ast_free(actual);
  source_close(src);
}


TEST(BuilderTest, DifferingSibling)
{
  source_t* src = source_open_string("Test");
  token_t* t = token_new(TK_INT, src);

  ast_t* expected = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 5);
  ast_add(expected, ast_new(t, TK_INT));

  token_set_int(t, 3);
  ast_add(expected, ast_new(t, TK_INT));

  ast_t* actual = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 4);
  ast_add(actual, ast_new(t, TK_INT));

  token_set_int(t, 3);
  ast_add(actual, ast_new(t, TK_INT));

  free_errors();

  ASSERT_FALSE(build_compare_asts(expected, actual));
  ASSERT_EQ(1, get_error_count());

  ast_free(expected);
  ast_free(actual);
  token_free(t);
  source_close(src);
}


TEST(BuilderTest, IgnoreSibling)
{
  source_t* src = source_open_string("Test");
  token_t* t = token_new(TK_INT, src);

  ast_t* expected = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 3);
  ast_add(expected, ast_new(t, TK_INT));

  ast_t* actual = ast_token(token_new(TK_PLUS, src));

  ast_add(actual, ast_new(t, TK_INT));

  ast_add_sibling(actual, ast_token(token_new(TK_MINUS, src)));

  ASSERT_TRUE(build_compare_asts_no_sibling(expected, actual));

  ast_free(expected);
  ast_free(actual);
  source_close(src);
}


TEST(BuilderTest, MissingType)
{
  source_t* src = source_open_string("Test");
  token_t* t = token_new(TK_INT, src);

  ast_t* expected = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 3);
  ast_settype(expected, ast_new(t, TK_INT));

  ast_t* actual = ast_token(token_new(TK_PLUS, src));

  free_errors();

  ASSERT_FALSE(build_compare_asts(expected, actual));
  ASSERT_EQ(1, get_error_count());

  ast_free(expected);
  ast_free(actual);
  token_free(t);
  source_close(src);
}


TEST(BuilderTest, UnexpectedType)
{
  source_t* src = source_open_string("Test");
  token_t* t = token_new(TK_INT, src);

  ast_t* expected = ast_token(token_new(TK_PLUS, src));

  ast_t* actual = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 3);
  ast_settype(actual, ast_new(t, TK_INT));

  free_errors();

  ASSERT_FALSE(build_compare_asts(expected, actual));
  ASSERT_EQ(1, get_error_count());

  ast_free(expected);
  ast_free(actual);
  source_close(src);
}


TEST(BuilderTest, DifferingType)
{
  source_t* src = source_open_string("Test");
  token_t* t = token_new(TK_INT, src);

  ast_t* expected = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 3);
  ast_settype(expected, ast_new(t, TK_INT));

  ast_t* actual = ast_token(token_new(TK_PLUS, src));

  token_set_int(t, 4);
  ast_settype(actual, ast_new(t, TK_INT));

  free_errors();

  ASSERT_FALSE(build_compare_asts(expected, actual));
  ASSERT_EQ(1, get_error_count());

  ast_free(expected);
  ast_free(actual);
  token_free(t);
  source_close(src);
}


TEST(BuilderTest, MissingScope)
{
  source_t* src = source_open_string("Test");

  ast_t* expected = ast_token(token_new(TK_PLUS, src));
  ast_scope(expected);

  ast_t* actual = ast_token(token_new(TK_MINUS, src));
  free_errors();

  ASSERT_FALSE(build_compare_asts(expected, actual));
  ASSERT_EQ(1, get_error_count());

  ast_free(expected);
  ast_free(actual);
  source_close(src);
}


TEST(BuilderTest, UnexpectedScope)
{
  source_t* src = source_open_string("Test");

  ast_t* expected = ast_token(token_new(TK_PLUS, src));

  ast_t* actual = ast_token(token_new(TK_MINUS, src));
  ast_scope(actual);

  free_errors();

  ASSERT_FALSE(build_compare_asts(expected, actual));
  ASSERT_EQ(1, get_error_count());

  ast_free(expected);
  ast_free(actual);
  source_close(src);
}
