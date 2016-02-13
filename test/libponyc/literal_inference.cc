#include <gtest/gtest.h>
#include "util.h"

// Literal type inference and limit checks

#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_COMPILE(src) DO(test_compile(src, "expr"))

class LiteralTest : public PassTest
{};

#define PROG_PREFIX "class Foo\n  new create() =>\n    "

// First the limitations

#if 0
TEST_F(LiteralTest, InferOpTyped)
{
  const char* src = PROG_PREFIX
    "let x: U8 = 2\n    "
    "let y: U8 = 256 / 2\n    "
    "let z: U8 = 256 / x";

  // Literal value is out of range for type (U8) #
  TEST_COMPILE(src);
}
#endif

TEST_F(LiteralTest, InferLitIs)
{
  const char* src = PROG_PREFIX "2 is 3";

  TEST_ERROR(src);
}

TEST_F(LiteralTest, InferLitIsLimitation)
{
  const char* src = PROG_PREFIX "2 is 2";

  TEST_ERROR(src);
  //TEST_COMPILE(src); // should work imho, PR #369
}

TEST_F(LiteralTest, InferLitIsNt)
{
  const char* src = PROG_PREFIX "2 isnt 2";

  TEST_ERROR(src);
}

TEST_F(LiteralTest, InferLitIsNtLimitation)
{
  const char* src = PROG_PREFIX "2 isnt 3";

  TEST_ERROR(src);
  //TEST_COMPILE(src); // should work imho, PR #369
}

TEST_F(LiteralTest, InferLitExpr)
{
  const char* src = PROG_PREFIX "(1 + 2) is 3";

  TEST_ERROR(src); // #369
}
