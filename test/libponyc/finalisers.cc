#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "finalisers"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "finalisers", errs)); }


class FinalisersTest : public PassTest
{};


TEST_F(FinalisersTest, FinalCanCreatePrimitive)
{
  const char* src =
    "primitive Prim\n"
    "primitive Foo\n"
    "  fun _final() =>\n"
    "    Prim";

  TEST_COMPILE(src);
}

TEST_F(FinalisersTest, FinalCannotCreateActor)
{
  const char* src =
    "actor Actor\n"
    "primitive Foo\n"
    "  fun _final() =>\n"
    "    Actor";

  TEST_ERRORS_1(src, "_final cannot create actors or send messages");
}

TEST_F(FinalisersTest, FinalCannotCreateActorInBranch)
{
  const char* src =
    "actor Actor\n"
    "primitive Foo\n"
    "  fun _final() =>\n"
    "    if false then None else Actor end";

  TEST_ERRORS_1(src, "_final cannot create actors or send messages");
}

TEST_F(FinalisersTest, FinalCannotCreateActorInTryBlock)
{
  const char* src =
    "actor Actor\n"
    "primitive Foo\n"
    "  fun _final() =>\n"
    "    try Actor; error end";

  TEST_ERRORS_1(src, "_final cannot create actors or send messages");
}

TEST_F(FinalisersTest, FinalCannotCallBehaviour)
{
  const char* src =
    "actor Actor\n"
    "  be apply() => None\n"
    "class Foo\n"
    "  let a: Actor = Actor\n"
    "  fun _final() =>\n"
    "    a()";

  TEST_ERRORS_1(src, "_final cannot create actors or send messages");
}

TEST_F(FinalisersTest, FinalCannotCallBehaviourInBranch)
{
  const char* src =
    "actor Actor\n"
    "  be apply() => None\n"
    "class Foo\n"
    "  let a: Actor = Actor\n"
    "  fun _final() =>\n"
    "    if true then a() end";

  TEST_ERRORS_1(src, "_final cannot create actors or send messages");
}

TEST_F(FinalisersTest, FinalCannotCallBehaviourInTryElseBlock)
{
  const char* src =
    "actor Actor\n"
    "  be apply() => None\n"
    "class Foo\n"
    "  let a: Actor = Actor\n"
    "  fun _final() =>\n"
    "    try error else a() end";

  TEST_ERRORS_1(src, "_final cannot create actors or send messages");
}
