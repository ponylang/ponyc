#include <gtest/gtest.h>
#include "util.h"

// Literal type inference and limit checks

#define TEST_AST(src, expect) DO(test_program_ast(src, "expr", expect))
#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_COMPILE(src) DO(test_compile(src, "expr"))


class LiteralTest : public PassTest
{};

#define PROG_PREFIX "actor Foo new create() => "

// Basic tests

TEST_F(LiteralTest, InferLitIs)
{
  const char* src = PROG_PREFIX "2 is 3";

  TEST_ERROR(src);
}

TEST_F(LiteralTest, InferLitIs1)
{
  const char* src = PROG_PREFIX "2 is 2";

  TEST_COMPILE(src);
}

TEST_F(LiteralTest, InferLitIsNt)
{
  const char* src = PROG_PREFIX "2 isnt 2";

  TEST_ERROR(src);
}

TEST_F(LiteralTest, InferLitIsNt1)
{
  const char* src = PROG_PREFIX "2 isnt 3";

  TEST_COMPILE(src);
}
