#include <gtest/gtest.h>
#include <platform.h>

#include <pkg/package.h>
#include <pkg/program.h>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "verify"))
#define TEST_ERROR(src) DO(test_error(src, "verify"))
#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }

class StableTypeTest : public PassTest
{};

TEST_F(StableTypeTest, EphemeralIsoLet)
{
  const char* src =
    "class A\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "        let x: A iso^ = recover A end";

  TEST_ERRORS_1(src,
      "")
}

TEST_F(StableTypeTest, EphemeralIsoVar)
{
  const char* src =
    "class A\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "        var x: A iso^ = recover A end";

  TEST_ERRORS_1(src,
      "")
}

TEST_F(StableTypeTest, EphemeralTrnLet)
{
  const char* src =
    "class A\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "        let x: A trn^ = recover A end";

  TEST_ERRORS_1(src,
      "")
}

TEST_F(StableTypeTest, EphemeralTrnVar)
{
  const char* src =
    "class A\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "        var x: A trn^ = recover A end";

  TEST_ERRORS_1(src,
      "")
}

TEST_F(StableTypeTest, IncompatibleIsectLet)
{
  const char* src =
    "class A\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "        let x: (A ref & A val) = recover A end";

  TEST_ERRORS_1(src,
      "")
}

TEST_F(StableTypeTest, IncompatibleIsectVar)
{
  const char* src =
    "class A\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "        var x: (A ref & A val) = recover A end";

  TEST_ERRORS_1(src,
      "")
}

TEST_F(StableTypeTest, GenericIncompatibleIsectUnstable)
{
  const char* src =
    "class Container[T: Any #read]\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "        let x: (A ref & A val) = recover A end";

  TEST_ERRORS_1(src,
      "")
}
