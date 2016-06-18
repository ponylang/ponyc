#include <gtest/gtest.h>
#include <platform.h>

#include <ast/token.h>
#include "../../src/libponyrt/mem/pool.h"


#define TEST_TOKEN_PRINT_ESCAPED(str, str_len, expected) \
  { token_t* token = token_new(TK_STRING); \
    token_set_string(token, str, str_len); \
    char* escaped = token_print_escaped(token); \
    EXPECT_TRUE(strcmp(expected, escaped) == 0) \
      << "Actual escaped string: " << escaped; \
    ponyint_pool_free_size(strlen(escaped), escaped); \
    token_free(token); }


class TokenTest: public testing::Test
{};


TEST(TokenTest, PrintEscapedString)
{
  TEST_TOKEN_PRINT_ESCAPED(
    "foo", 3,
    "foo");
}


TEST(TokenTest, PrintEscapedStringContainingDoubleQuotes)
{
  TEST_TOKEN_PRINT_ESCAPED(
    "foo\"bar\"baz", 11,
    "foo\\\"bar\\\"baz");
}


TEST(TokenTest, PrintEscapedStringContainingBackslashes)
{
  TEST_TOKEN_PRINT_ESCAPED(
    "foo\\bar\\baz", 11,
    "foo\\\\bar\\\\baz");
}


TEST(TokenTest, PrintEscapedStringContainingNullBytes)
{
  TEST_TOKEN_PRINT_ESCAPED(
    "foo\0bar\0baz", 11,
    "foo\\0bar\\0baz");
}
