#include <gtest/gtest.h>
#include <platform.h>

#include <type/subtype.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "ir"))

#define TEST_ERROR(src, err) \
  { const char* errs[] = {err, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }


class BareTest : public PassTest
{};


TEST_F(BareTest, BareMethod_Simple)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo()\n"

    "  fun @foo() =>\n"
    "    None";

  TEST_COMPILE(src);
}


TEST_F(BareTest, BareMethod_ConstructorCantBeBare)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo()\n"

    "  new @foo() =>\n"
    "    None";

  TEST_ERROR(src, "actor constructor cannot specify bareness");
}


TEST_F(BareTest, BareMethod_BehaviourCantBeBare)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo()\n"

    "  be @foo() =>\n"
    "    None";

  TEST_ERROR(src, "actor behaviour cannot specify bareness");
}


TEST_F(BareTest, BareMethod_ThisReference)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo()\n"

    "  fun @foo() =>\n"
    "    this";

  TEST_ERROR(src, "can't reference 'this' in a bare method");
}


TEST_F(BareTest, BareMethod_Subtyping)
{
  const char* src =
    "interface I1\n"
    "  fun @foo()\n"

    "interface I2\n"
    "  fun bar()\n"

    "interface I3\n"
    "  fun foo()\n"

    "interface I4\n"
    "  fun @bar()\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let t = this\n"
    "    let o = object ref\n"
    "      fun foo() => None\n"
    "      fun @bar() => None\n"
    "    end\n"

    "    let i1: I1 = t\n"
    "    let i2: I2 = t\n"
    "    let i3: I3 = o\n"
    "    let i4: I4 = o\n"

    "  fun @foo() =>\n"
    "    None\n"

    "  fun bar() =>\n"
    "    None";

  TEST_COMPILE(src);

  ASSERT_FALSE(is_subtype(type_of("t"), type_of("i3"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("t"), type_of("i4"), NULL, &opt));
}


TEST_F(BareTest, BareMethod_AbstractCall)
{
  const char* src =
    "interface I\n"
    "  fun @foo()\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    I.foo()";

  TEST_ERROR(src, "a bare method cannot be called on an abstract type "
    "reference");
}


TEST_F(BareTest, BareMethod_PartialApplicationYieldsBareLambda)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: @{()} = this~foo()\n"

    "  fun @foo() => None";

  TEST_COMPILE(src);
}


TEST_F(BareTest, BareMethod_PartialApplicationArguments)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: @{()} = this~foo(42)\n"

    "  fun @foo(x: U8) => None";

  TEST_ERROR(src, "the partial application of a bare method cannot take "
    "arguments");
}


TEST_F(BareTest, BareMethod_PartialApplicationTypeArguments)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: @{()} = this~foo()\n"

    "  fun @foo[A](x: U8) => None";

  TEST_ERROR(src, "not enough type arguments");
}


TEST_F(BareTest, BareLambda_Simple)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: @{()} = @{() => None}";

  TEST_COMPILE(src);
}


TEST_F(BareTest, BareLambda_IsVal)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: @{()} val = @{() => None}";

  TEST_COMPILE(src);
}

TEST_F(BareTest, BareLambda_CantChangeCap)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: @{()} = @{() => None} ref";

  TEST_ERROR(src, "a bare lambda can only have a 'val' capability");
}


TEST_F(BareTest, BareLambda_TypeCantChangeCap)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: @{()} ref = @{() => None}";

  TEST_ERROR(src, "a bare lambda can only have a 'val' capability");
}


TEST_F(BareTest, BareLambda_Captures)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: @{()} = @{()(env) => None}";

  TEST_ERROR(src, "a bare lambda cannot specify captures");
}


TEST_F(BareTest, BareLambda_ThisReference)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: @{()} = @{() => this}";

  TEST_ERROR(src, "can't reference 'this' in a bare method");
}


TEST_F(BareTest, BareLambda_ReceiverCap)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x: @{()} = @{ref() => None}";

  TEST_ERROR(src, "a bare lambda cannot specify a receiver capability");
}


TEST_F(BareTest, BareLambda_TypeParameters)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let x = @{[A]() => None}";

  TEST_ERROR(src, "a bare lambda cannot specify type parameters");
}


TEST_F(BareTest, BareLambda_TypeTypeParameters)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    var x: @{[A]()}";

  TEST_ERROR(src, "a bare lambda cannot specify type parameters");
}


TEST_F(BareTest, BareLambda_Subtyping)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let concrete_bare = @{() => None}\n"
    "    let concrete_nonbare = {() => None}\n"
    "    let interface_bare: @{()} = concrete_bare\n"
    "    let interface_nonbare: {()} val = concrete_nonbare";

  TEST_COMPILE(src);

  ASSERT_FALSE(is_subtype(type_of("concrete_bare"),
    type_of("interface_nonbare"), NULL, &opt));
  ASSERT_FALSE(is_subtype(type_of("concrete_nonbare"),
    type_of("interface_bare"), NULL, &opt));
}


TEST_F(BareTest, BareType_Union)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a: (U8 | @{()}) = @{() => None}";

  TEST_ERROR(src, "right side must be a subtype of left side");
}


TEST_F(BareTest, BareType_Match)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let a: (U8 | @{()}) = 42\n"
    "    match a\n"
    "    | let _: U8 => None\n"
    "    end";

  TEST_ERROR(src, "a match operand cannot have a bare type");
}


TEST_F(BareTest, BareType_TypeArgument)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    foo[@{()}]()\n"

    "  fun foo[A]() => None";

  TEST_ERROR(src, "a bare type cannot be used as a type argument");
}
