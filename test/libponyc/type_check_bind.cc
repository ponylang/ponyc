#include <gtest/gtest.h>
#include <platform.h>
#include <type/subtype.h>
#include <type/typeparam.h>
#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))

class BindTest: public PassTest
{};


TEST_F(BindTest, RecursiveConstraintIsUnbound)
{
  const char* src =
    "class C1[A, B: A]\n"
    "  var x: B\n"
    "  new create(x': B) => x = consume x'\n";

  TEST_COMPILE(src);
  ast_t* x = lookup_member("C1", "x");
  ASSERT_EQ(NULL, typeparam_constraint(ast_type(x)));
}
