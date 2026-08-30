#include <gtest/gtest.h>
#include <platform.h>

#include <ast/ast.h>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_EQUIV(src, expect) DO(test_equiv(src, "expr", expect, "expr"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }

class TypeParamsTest : public PassTest
{};

TEST_F(TypeParamsTest, DefaultCapInConstraint_ClassGetsRef)
{
  // From issue #5116. A bare class name in a constraint uses the class's
  // default cap (ref), so a ref-receiver method is callable.
  const char* src =
    "class C\n"
    "  fun ref mutate() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[C](C)\n"

    "  fun foo[A: C](x: A) =>\n"
    "    x.mutate()";

  TEST_COMPILE(src);
}

TEST_F(TypeParamsTest, DefaultCapInConstraint_ValNotSubtypeOfRef)
{
  // From issue #5116. A bare class name in a constraint uses the class's
  // default cap (ref). A val type argument does not satisfy a ref constraint.
  const char* src =
    "class C\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let c: C val = recover val C end\n"
    "    foo[C val](c)\n"

    "  fun foo[A: C](x: A) => None";

  TEST_ERRORS_1(src, "type argument is outside its constraint");
}

TEST_F(TypeParamsTest, ReifySimultaneously)
{
  // From issue #1870
  const char* src =
    "interface State[S, I, O]\n"
    " fun val apply(state: S, input: I): (S, O)\n"
    " fun val bind[O2](next: State[S, O, O2]): State[S, I, O2]\n";

  TEST_COMPILE(src);
}
