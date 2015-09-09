#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


// Tests for sugar that is applied during the expr pass

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_EQUIV(src, expect) DO(test_equiv(src, "expr", expect, "expr"))


class SugarExprTest : public PassTest
{};


/*
TEST_F(SugarExprTest, PartialWithTypeArgs)
{
  const char* short_form =
    "class Foo ref\n"
    "  fun f[A]() =>\n"
    "    this~f[A]()";

  TEST_ERROR(short_form);
}
*/


TEST_F(SugarExprTest, LambdaMinimal)
{
  const char* short_form =
    "class Foo ref\n"
    "  fun f() =>\n"
    "    lambda() => None end";

  const char* full_form =
    "class Foo ref\n"
    "  fun f() =>\n"
    "    object\n"
    "      fun apply() => None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, LambdaFull)
{
  const char* short_form =
    "trait A\n"
    "trait B\n"
    "trait C\n"
    "trait D\n"
    "trait D2 is D\n"

    "class Foo\n"
    "  fun f(c: C val, d: D2 val) =>\n"
    "    lambda iso (a: A, b: B)(c, _c = c, _d: D val = d): A => a end";

  const char* full_form =
    "trait A\n"
    "trait B\n"
    "trait C\n"
    "trait D\n"
    "trait D2 is D\n"

    "class Foo\n"
    "  fun f(c: C val, d: D2 val) =>\n"
    "    object iso\n"
    "      let c: C val = c\n"
    "      let _c: C val = c\n"
    "      let _d: D val = d\n"
    "      fun apply(a: A, b: B): A => a\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, LambdaThrow)
{
  const char* short_form =
    "class Foo ref\n"
    "  fun f() =>\n"
    "    lambda() ? => error end";

  const char* full_form =
    "class Foo ref\n"
    "  fun f() =>\n"
    "    object\n"
    "      fun apply() ? => error\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, LambdaWithTypeArgs)
{
  const char* short_form =
    "trait T\n"

    "class Foo ref\n"
    "  fun f() =>\n"
    "    lambda[A: T]() => None end";

  const char* full_form =
    "trait T\n"

    "class Foo\n"
    "  fun f() =>\n"
    "    object\n"
    "      fun apply[A: T]() => None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, LambdaCaptureLocalLet)
{
  const char* short_form =
    "trait T\n"

    "class Foo ref\n"
    "  fun f() =>\n"
    "    let x: U32 = 4\n"
    "    lambda()(x) => None end";

  const char* full_form =
    "trait T\n"

    "class Foo\n"
    "  fun f() =>\n"
    "    let x: U32 = 4\n"
    "    object\n"
    "      let x: U32 = x"
    "      fun apply() => None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, LambdaCaptureLocalVar)
{
  const char* short_form =
    "trait T\n"

    "class Foo ref\n"
    "  fun f() =>\n"
    "    var x: U32 = 4\n"
    "    lambda()(x) => None end";

  const char* full_form =
    "trait T\n"

    "class Foo\n"
    "  fun f() =>\n"
    "    var x: U32 = 4\n"
    "    object\n"
    "      let x: U32 = x"
    "      fun apply() => None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, LambdaCaptureParameter)
{
  const char* short_form =
    "trait T\n"

    "class Foo ref\n"
    "  fun f(x: U32) =>\n"
    "    lambda()(x) => None end";

  const char* full_form =
    "trait T\n"

    "class Foo\n"
    "  fun f(x: U32) =>\n"
    "    object\n"
    "      let x: U32 = x"
    "      fun apply() => None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, LambdaCaptureFieldLet)
{
  const char* short_form =
    "trait T\n"

    "class Foo ref\n"
    "  let x: U32 = 4\n"
    "  fun f() =>\n"
    "    lambda()(x) => None end";

  const char* full_form =
    "trait T\n"

    "class Foo\n"
    "  let x: U32 = 4\n"
    "  fun f() =>\n"
    "    object\n"
    "      let x: U32 = x"
    "      fun apply() => None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, LambdaCaptureFieldVar)
{
  const char* short_form =
    "trait T\n"

    "class Foo ref\n"
    "  var x: U32 = 4\n"
    "  fun f() =>\n"
    "    lambda()(x) => None end";

  const char* full_form =
    "trait T\n"

    "class Foo\n"
    "  var x: U32 = 4\n"
    "  fun f() =>\n"
    "    object\n"
    "      let x: U32 = x"
    "      fun apply() => None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}
