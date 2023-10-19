#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "expr"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }

class WithTest : public PassTest
{};

TEST_F(WithTest, NoEarlyReturnFromWith)
{
  // From issue #4464
  const char* src =
    "class Disposable\n"
    "  new create() => None\n"
    "  fun ref set_exit(x: U32) => None\n"
    "  fun dispose() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    with d = Disposable do\n"
    "      d.set_exit(10)\n"
    "      return\n"
    "    end";

  TEST_ERRORS_1(src,
    "use return only to exit early from a with block, not at the end");
}
