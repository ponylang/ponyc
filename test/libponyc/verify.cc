#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "verify"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "verify", errs)); }

#define TEST_ERRORS_2(src, err1, err2) \
  { const char* errs[] = {err1, err2, NULL}; \
    DO(test_expected_errors(src, "verify", errs)); }

#define TEST_ERRORS_3(src, err1, err2, err3) \
  { const char* errs[] = {err1, err2, err3, NULL}; \
    DO(test_expected_errors(src, "verify", errs)); }


class VerifyTest : public PassTest
{};


TEST_F(VerifyTest, MainActorFunCreate)
{
  // From issue #906
  const char* src =
    "actor Main\n"
    "  fun create(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "the create method of the Main actor must be a constructor");
}

TEST_F(VerifyTest, MainActorNewCreateTypeParams)
{
  const char* src =
    "actor Main\n"
    "  new create[A: Number](env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "the create constructor of the Main actor must not take type parameters");
}

TEST_F(VerifyTest, MainActorNewCreateTwoParams)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env, bogus: Bool) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "the create constructor of the Main actor must take only a single Env"
    "parameter");
}

TEST_F(VerifyTest, MainActorNewCreateParamWrongType)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Bool) =>\n"
    "    None";

  TEST_ERRORS_1(src, "must be of type Env");
}

TEST_F(VerifyTest, MainActorNoConstructor)
{
  // From issue #1259
  const char* src =
    "actor Main\n";

  TEST_ERRORS_1(src,
    "The Main actor must have a create constructor which takes only a single "
    "Env parameter");
}

TEST_F(VerifyTest, PrimitiveWithTypeParamsInit)
{
  const char* src =
    "primitive Foo[A: Number]\n"
    "  fun _init() =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a primitive with type parameters cannot have an _init method");
}

TEST_F(VerifyTest, PrimitiveNewInit)
{
  const char* src =
    "primitive Foo\n"
    "  new _init() =>\n"
    "    None";

  TEST_ERRORS_3(src,
    "a primitive _init method must be a function",
    "a primitive _init method must use box as the receiver capability",
    "a primitive _init method must return None");
}

TEST_F(VerifyTest, PrimitiveFunRefInit)
{
  const char* src =
    "primitive Foo\n"
    "  fun ref _init() =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a primitive _init method must use box as the receiver capability");
}

TEST_F(VerifyTest, PrimitiveInitWithTypeParams)
{
  const char* src =
    "primitive Foo\n"
    "  fun _init[A: Number]() =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a primitive _init method must not take type parameters");
}

TEST_F(VerifyTest, PrimitiveInitWithParams)
{
  const char* src =
    "primitive Foo\n"
    "  fun _init(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a primitive _init method must take no parameters");
}

TEST_F(VerifyTest, PrimitiveInitReturnBool)
{
  const char* src =
    "primitive Foo\n"
    "  fun _init(): Bool =>\n"
    "    true";

  TEST_ERRORS_1(src,
    "a primitive _init method must return None");
}

TEST_F(VerifyTest, PrimitiveInitPartial)
{
  const char* src =
    "primitive Foo\n"
    "  fun _init() ? =>\n"
    "    error";

  TEST_ERRORS_1(src,
    "a primitive _init method cannot be a partial function");
}

TEST_F(VerifyTest, PrimitiveWithTypeParamsFinal)
{
  const char* src =
    "primitive Foo[A: Number]\n"
    "  fun _final() =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a primitive with type parameters cannot have a _final method");
}

TEST_F(VerifyTest, ClassWithTypeParamsFinal)
{
  const char* src =
    "class Foo[A: Number]\n"
    "  fun _final() =>\n"
    "    None";

  TEST_COMPILE(src);
}

TEST_F(VerifyTest, ActorBeFinal)
{
  const char* src =
    "actor Foo\n"
    "  be _final() =>\n"
    "    None";

  TEST_ERRORS_3(src,
    "a _final method must be a function",
    "a _final method must use box as the receiver capability",
    "a _final method must return None");
}

TEST_F(VerifyTest, ClassFunRefFinal)
{
  const char* src =
    "class Foo\n"
    "  fun ref _final() =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a _final method must use box as the receiver capability");
}

TEST_F(VerifyTest, ClassFinalWithTypeParams)
{
  const char* src =
    "class Foo\n"
    "  fun _final[A: Number]() =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a _final method must not take type parameters");
}

TEST_F(VerifyTest, ClassFinalWithParams)
{
  const char* src =
    "class Foo\n"
    "  fun _final(env: Env) =>\n"
    "    None";

  TEST_ERRORS_1(src,
    "a _final method must take no parameters");
}

TEST_F(VerifyTest, ClassFinalReturnBool)
{
  const char* src =
    "class Foo\n"
    "  fun _final(): Bool =>\n"
    "    true";

  TEST_ERRORS_1(src,
    "a _final method must return None");
}

TEST_F(VerifyTest, ClassFinalPartial)
{
  const char* src =
    "class Foo\n"
    "  fun _final() ? =>\n"
    "    error";

  TEST_ERRORS_1(src,
    "a _final method cannot be a partial function");
}
