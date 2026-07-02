#include <gtest/gtest.h>
#include <platform.h>

#include <ast/token.h>
#include <ast/stringtab.h>
#include "../../src/libponyrt/mem/pool.h"


#define TEST_TOKEN_PRINT_ESCAPED(str, str_len, expected) \
  { token_t* token = token_new(TK_STRING); \
    token_set_string(token, str, str_len, tab); \
    char* escaped = token_print_escaped(token); \
    EXPECT_TRUE(strcmp(expected, escaped) == 0) \
      << "Actual escaped string: " << escaped; \
    ponyint_pool_free_size(strlen(escaped), escaped); \
    token_free(token); }


class TokenTest: public testing::Test
{
protected:
  strtable_t* tab;
  void SetUp() override { tab = stringtab_new(); }
  void TearDown() override { stringtab_free(tab); }
};


TEST_F(TokenTest, PrintEscapedString)
{
  TEST_TOKEN_PRINT_ESCAPED(
    "foo", 3,
    "foo");
}


TEST_F(TokenTest, PrintEscapedStringContainingDoubleQuotes)
{
  TEST_TOKEN_PRINT_ESCAPED(
    "foo\"bar\"baz", 11,
    "foo\\\"bar\\\"baz");
}


TEST_F(TokenTest, PrintEscapedStringContainingBackslashes)
{
  TEST_TOKEN_PRINT_ESCAPED(
    "foo\\bar\\baz", 11,
    "foo\\\\bar\\\\baz");
}


TEST_F(TokenTest, PrintEscapedStringContainingNullBytes)
{
  TEST_TOKEN_PRINT_ESCAPED(
    "foo\0bar\0baz", 11,
    "foo\\0bar\\0baz");
}
