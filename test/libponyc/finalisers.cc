#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "final"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "final", errs)); }


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

TEST_F(FinalisersTest, FinalCannotCallChainedBehaviour)
{
  const char* src =
    "actor Actor\n"
    "  be apply() => None\n"
    "class Foo\n"
    "  let a: Actor = Actor\n"
    "  fun _final() =>\n"
    "    a.>apply()";

  TEST_ERRORS_1(src, "_final cannot create actors or send messages");
}

TEST_F(FinalisersTest, FinalCanCallMethodOnGenericClassWithConcreteTypeArgs)
{
  const char* src =
    "trait Gettable\n"
    "  fun get(): U32\n"
    "primitive Prim is Gettable\n"
    "  fun get(): U32 => 0\n"
    "class Generic[A: Gettable val]\n"
    "  let _value: A\n"
    "  new val create(value: A) =>\n"
    "    _value = value\n"
    "  fun get(): U32 =>\n"
    "    _value.get()\n"
    "class Foo\n"
    "  fun _final() =>\n"
    "    Generic[Prim](Prim).get()";

  TEST_COMPILE(src);
}

TEST_F(FinalisersTest, FinalCannotCallMethodOnGenericClassThatSends)
{
  // The send_msg method sends a message via a behavior call on an actor field.
  // Even though Generic is instantiated with concrete type args (U32),
  // the finaliser pass should still detect the send inside send_msg.
  const char* src =
    "actor Ping\n"
    "  new create() => None\n"
    "  be ping() => None\n"
    "class Holder\n"
    "  let _p: Ping\n"
    "  new create(p: Ping) => _p = p\n"
    "  fun get(): Ping => _p\n"
    "class Generic[A]\n"
    "  let _h: Holder\n"
    "  new create(h: Holder) => _h = h\n"
    "  fun send_msg() =>\n"
    "    _h.get().ping()\n"
    "class Foo\n"
    "  let _g: Generic[U32]\n"
    "  new create(h: Holder) => _g = Generic[U32](h)\n"
    "  fun _final() =>\n"
    "    _g.send_msg()";

  TEST_ERRORS_1(src, "_final cannot create actors or send messages");
}

TEST_F(FinalisersTest, CannotLookupFinal)
{
  const char* src =
    "class Actor\n"
    "  fun apply() =>\n"
    "    _final()\n"
    "  fun _final() =>\n"
    "    None";

  TEST_ERRORS_1(src, "can't lookup a _final function");
}

TEST_F(FinalisersTest, CannotLookupPrimitiveInit)
{
  const char* src =
    "primitive Prim\n"
    "  fun apply() =>\n"
    "    _init()\n"
    "  fun _init() =>\n"
    "    None";

  TEST_ERRORS_1(src, "can't lookup an _init function on a primitive");
}
