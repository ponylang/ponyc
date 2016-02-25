#include <gtest/gtest.h>
#include "util.h"

// Literal type inference and limit checks.

#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_COMPILE(src) DO(test_compile(src, "expr"))

class LiteralTest : public PassTest
{};


// First the limitations.

TEST_F(LiteralTest, InferLitIs)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    2 is 3";

  // Fails because type of literals cannot be determined. See #369.
  TEST_ERROR(src);
}


TEST_F(LiteralTest, InferLitIsnt)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    2 isnt 2";

  // Fails because type of literals cannot be determined. See #369.
  TEST_ERROR(src);
}


TEST_F(LiteralTest, InferLitExpr)
{
  const char* src =
    "class Foo\n"
    "  new create() =>\n"
    "    (1 + 2) is 3";

  // Fails because type of literals cannot be determined. See #369.
  TEST_ERROR(src);
}
