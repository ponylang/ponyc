#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


// Tests for sugar that is applied during the expr pass

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_EQUIV(src, expect) DO(test_equiv(src, "expr", expect, "expr"))


class SugarExprTest : public PassTest
{};


// Partial application

TEST_F(SugarExprTest, PartialMinimal)
{
  const char* short_form =
    "trait T\n"
    "  fun f()\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    x~f()";

  const char* full_form =
    "trait T\n"
    "  fun f()\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    {box()($1 = x) => $1.f() }";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, PartialReceiverCapability)
{
  const char* short_form =
    "trait T\n"
    "  fun ref f()\n"

    "class Foo\n"
    "  fun f(x: T ref) =>\n"
    "    x~f()";

  const char* full_form =
    "trait T\n"
    "  fun ref f()\n"

    "class Foo\n"
    "  fun f(x: T ref) =>\n"
    "    {ref()($1 = x) => $1.f() }";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, PartialReceiverCapabilityFail)
{
  const char* short_form =
    "trait T\n"
    "  fun ref f()\n"

    "class Foo\n"
    "  fun f(x: T box) =>\n"
    "    x~f()";

  TEST_ERROR(short_form);
}


TEST_F(SugarExprTest, PartialMemberAccessFail)
{
  const char* short_form =
    "class Foo\n"
    "  let y: U32 = 4"
    "  fun f() =>\n"
    "    this~y";

  TEST_ERROR(short_form);
}


TEST_F(SugarExprTest, PartialTupleElementAccessFail)
{
  const char* short_form =
    "trait T\n"
    "  fun f()\n"

    "class Foo\n"
    "  fun f(x: (T, T)) =>\n"
    "    x~_1()";

  TEST_ERROR(short_form);
}


TEST_F(SugarExprTest, PartialSomeArgs)
{
  const char* short_form =
    "trait T\n"
    "  fun f(a: U32, b: U32, c: U32, d: U32)\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    x~f(3 where c = 4)";

  const char* full_form =
    "trait T\n"
    "  fun f(a: U32, b: U32, c: U32, d: U32)\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    {box(b: U32, d: U32)\n"
    "      ($1 = x, a: U32 = (3), c: U32 = (4)) =>\n"
    "      $1.f(a, consume b, c, consume d)\n"
    "    }";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, PartialAllArgs)
{
  const char* short_form =
    "trait T\n"
    "  fun f(a: U32, b: U32, c: U32, d: U32)\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    x~f(1, 2, 3, 4)";

  const char* full_form =
    "trait T\n"
    "  fun f(a: U32, b: U32, c: U32, d: U32)\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    {box()($1 = x, a: U32 = (1), b: U32 = (2), c: U32 = (3),\n"
    "      d: U32 = (4)) =>\n"
    "      $1.f(a, b, c, d)\n"
    "    }";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, PartialDefaultArgs)
{
  const char* short_form =
    "trait T\n"
    "  fun f(a: U32 = 1, b: U32 = 2, c: U32 = 3, d: U32 = 4)\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    x~f(5 where c = 6)";

  const char* full_form =
    "trait T\n"
    "  fun f(a: U32 = 1, b: U32 = 2, c: U32 = 3, d: U32 = 4)\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    {box(b: U32 = 2, d: U32 = 4)\n"
    "      ($1 = x, a: U32 = (5), c: U32 = (6)) =>\n"
    "      $1.f(a, consume b, c, consume d)\n"
    "    }";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, PartialResult)
{
  const char* short_form =
    "trait T\n"
    "  fun f(): U32\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    x~f()";

  const char* full_form =
    "trait T\n"
    "  fun f(): U32\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    {box()($1 = x): U32 => $1.f() }";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, PartialBehaviour)
{
  const char* short_form =
    "trait T\n"
    "  be f()\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    x~f()";

  const char* full_form =
    "trait T\n"
    "  be f()\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    {box()($1 = x) => $1.f() }";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, PartialRaisesError)
{
  const char* short_form =
    "trait T\n"
    "  fun f(): U32 ?\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    x~f()";

  const char* full_form =
    "trait T\n"
    "  fun f(): U32 ?\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    {box()($1 = x): U32 ? => $1.f() }";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, PartialParameterTypeParameters)
{
  const char* short_form =
    "trait T\n"
    "  fun f(a: T box)\n"

    "class Foo[A: T #read]\n"
    "  fun f(t: T box, a: A) =>\n"
    "    t~f(a)";

  const char* full_form =
    "trait T\n"
    "  fun f(a: T box)\n"

    "class Foo[A: T #read]\n"
    "  fun f(t: T box, a: A) =>\n"
    "    {box()($1 = t, a: A = (a)) => $1.f(a) }";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, PartialReceiverTypeParameters)
{
  const char* short_form =
    "trait T\n"
    "  fun f()\n"

    "class Foo[A: T #read]\n"
    "  fun f(x: A) =>\n"
    "    x~f()";

  const char* full_form =
    "trait T\n"
    "  fun f()\n"

    "class Foo[A: T #read]\n"
    "  fun f(x: A) =>\n"
    "    {box()($1 = x) => $1.f() }";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, PartialFunctionTypeParameter)
{
  const char* short_form =
    "trait T\n"
    "  fun f[A](a: A)\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    x~f[U32](3)";

  const char* full_form =
    "trait T\n"
    "  fun f[A](a: A)\n"

    "class Foo\n"
    "  fun f(x: T) =>\n"
    "    {box()($1 = x, a: U32 = (3)) => $1.f[U32](a) }";

  TEST_EQUIV(short_form, full_form);
}


// Lambdas

TEST_F(SugarExprTest, LambdaMinimal)
{
  const char* short_form =
    "class Foo\n"
    "  fun f() =>\n"
    "    {() => None }";

  const char* full_form =
    "class Foo\n"
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
    "    {ref(a: A, b: B)(c, _c = c, _d: D val = d): A => a } iso";

  const char* full_form =
    "trait A\n"
    "trait B\n"
    "trait C\n"
    "trait D\n"
    "trait D2 is D\n"

    "class Foo\n"
    "  fun f(c: C val, d: D2 val) =>\n"
    "    object iso\n"
    "      var c: C val = c\n"
    "      var _c: C val = c\n"
    "      var _d: D val = d\n"
    "      fun ref apply(a: A, b: B): A => a\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, LambdaThrow)
{
  const char* short_form =
    "class Foo\n"
    "  fun f() =>\n"
    "    {() ? => error }";

  const char* full_form =
    "class Foo\n"
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

    "class Foo\n"
    "  fun f() =>\n"
    "    {[A: T]() => None }";

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

    "class Foo\n"
    "  fun f() =>\n"
    "    let x: U32 = 4\n"
    "    {()(x) => None }";

  const char* full_form =
    "trait T\n"

    "class Foo\n"
    "  fun f() =>\n"
    "    let x: U32 = 4\n"
    "    object\n"
    "      var x: U32 = x"
    "      fun apply() => None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, LambdaCaptureLocalVar)
{
  const char* short_form =
    "trait T\n"

    "class Foo\n"
    "  fun f() =>\n"
    "    var x: U32 = 4\n"
    "    {()(x) => None }";

  const char* full_form =
    "trait T\n"

    "class Foo\n"
    "  fun f() =>\n"
    "    var x: U32 = 4\n"
    "    object\n"
    "      var x: U32 = x"
    "      fun apply() => None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, LambdaCaptureParameter)
{
  const char* short_form =
    "trait T\n"

    "class Foo\n"
    "  fun f(x: U32) =>\n"
    "    {()(x) => None }";

  const char* full_form =
    "trait T\n"

    "class Foo\n"
    "  fun f(x: U32) =>\n"
    "    object\n"
    "      var x: U32 = x"
    "      fun apply() => None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, LambdaCaptureFieldLet)
{
  const char* short_form =
    "trait T\n"

    "class Foo\n"
    "  let x: U32 = 4\n"
    "  fun f() =>\n"
    "    {()(x) => None }";

  const char* full_form =
    "trait T\n"

    "class Foo\n"
    "  let x: U32 = 4\n"
    "  fun f() =>\n"
    "    object\n"
    "      var x: U32 = x"
    "      fun apply() => None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, LambdaCaptureFieldVar)
{
  const char* short_form =
    "trait T\n"

    "class Foo\n"
    "  var x: U32 = 4\n"
    "  fun f() =>\n"
    "    {()(x) => None }";

  const char* full_form =
    "trait T\n"

    "class Foo\n"
    "  var x: U32 = 4\n"
    "  fun f() =>\n"
    "    object\n"
    "      var x: U32 = x"
    "      fun apply() => None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, LambdaNamed)
{
  const char* short_form =
    "class Foo\n"
    "  fun f() =>\n"
    "    {bar() => None }";

  const char* full_form =
    "class Foo\n"
    "  fun f() =>\n"
    "    object\n"
    "      fun bar() => None\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, Location)
{
  const char* short_form =
    "class Foo\n"
    "  var create: U32\n"
    "  fun bar() =>\n"
    "    __loc";

  const char* full_form =
    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box bar(): None =>\n"
    "    object\n"
    "      fun tag file(): String => \"\"\n"
    "      fun tag method(): String => \"bar\"\n"
    "      fun tag line(): USize => 4\n"
    "      fun tag pos(): USize => 5\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, LocationDefaultArg)
{
  const char* short_form =
    "interface val SourceLoc\n"
    "  fun file(): String\n"
    "  fun method(): String\n"
    "  fun line(): USize\n"
    "  fun pos(): USize\n"

    "class Foo\n"
    "  var create: U32\n"
    "  fun bar() =>\n"
    "    wombat()\n"
    "  fun wombat(x: SourceLoc = __loc) =>\n"
    "    None";

  const char* full_form =
    "interface val SourceLoc\n"
    "  fun file(): String\n"
    "  fun method(): String\n"
    "  fun line(): USize\n"
    "  fun pos(): USize\n"

    "class Foo\n"
    "  var create: U32\n"
    "  fun bar() =>\n"
    "    wombat(object\n"
    "      fun tag file(): String => \"\"\n"
    "      fun tag method(): String => \"bar\"\n"
    "      fun tag line(): USize => 9\n"
    "      fun tag pos(): USize => 12\n"
    "    end)\n"
    "  fun wombat(x: SourceLoc = __loc) =>\n"
    "    None";

  TEST_EQUIV(short_form, full_form);
}


// Early sugar that may cause errors in type checking

TEST_F(SugarExprTest, ObjectLiteralReferencingTypeParameters)
{
  const char* short_form =
    "trait T\n"

    "class Foo[A: T]\n"
    "  fun f(x: A) =>\n"
    "    object let _x: A = consume x end";

  const char* full_form =
    "trait T\n"

    "class Foo[A: T]\n"
    "  fun f(x: A) =>\n"
    "    $T[A].create(consume x)\n"

    "class $T[A: T]\n"
    "  let _x: A\n"
    "  new create($1: A) =>\n"
    "    _x = consume $1";

  TEST_EQUIV(short_form, full_form);
}
