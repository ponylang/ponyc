#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "expr"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }

#define TEST_ERRORS_2(src, err1, err2) \
  { const char* errs[] = {err1, err2, NULL}; \
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

TEST_F(WithTest, DontcareInTupleSecondBindingIsRejected)
{
  // Regression: build_with_dispose must recurse through every tuple element.
  // A prior bug returned after the first element, so `_` in a later position
  // was not diagnosed.
  const char* src =
    "class D\n"
    "  new create() => None\n"
    "  fun dispose() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    with (a, _) = (D.create(), D.create()) do\n"
    "      None\n"
    "    end";

  TEST_ERRORS_1(src, "_ isn't allowed for a variable in a with block");
}

TEST_F(WithTest, BothDontcareInTupleIsRejected)
{
  // Error accumulation: both `_` bindings should be reported, not just the
  // first one.
  const char* src =
    "class D\n"
    "  new create() => None\n"
    "  fun dispose() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    with (_, _) = (D.create(), D.create()) do\n"
    "      None\n"
    "    end";

  TEST_ERRORS_2(src,
    "_ isn't allowed for a variable in a with block",
    "_ isn't allowed for a variable in a with block");
}

TEST_F(WithTest, TwoElementTupleWithCompiles)
{
  // Happy path: both bindings get dispose calls. The tuple loop must process
  // every element (regression for early-return bug).
  const char* src =
    "class D\n"
    "  new create() => None\n"
    "  fun dispose() => None\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    with (a, b) = (D.create(), D.create()) do\n"
    "      None\n"
    "    end";

  TEST_COMPILE(src);
}
