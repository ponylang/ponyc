#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


// Tests for sugar that is applied during the expr pass

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_EQUIV(src, expect) DO(test_equiv(src, "expr", expect, "expr"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }


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
    "    {box()($1 = x): U32 ? => $1.f()? }";

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
    "      fun tag pos(): USize => 11\n"
    "    end)\n"
    "  fun wombat(x: SourceLoc = __loc) =>\n"
    "    None";

  TEST_EQUIV(short_form, full_form);
}


// Exhaustive Match

TEST_F(SugarExprTest, MatchExhaustiveAllCasesOfUnion)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"
    "trait T3\n"

    "primitive Foo\n"
    "  fun apply(t': (T1 | T2 | T3)): Bool =>\n"
    "    match t'\n"
    "    | let t: T1 => true\n"
    "    | let t: T2 => false\n"
    "    | let t: T3 => true\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(SugarExprTest, MatchNonExhaustiveSomeCasesMissing)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"
    "trait T3\n"

    "primitive Foo\n"
    "  fun apply(t': (T1 | T2 | T3)): Bool =>\n"
    "    match t'\n"
    "    | let t: T1 => true\n"
    "    | let t: T2 => false\n"
    "    end";

  TEST_ERRORS_1(src, "function body isn't the result type");
}


TEST_F(SugarExprTest, MatchNonExhaustiveSomeCasesGuarded)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"
    "trait T3 fun gt(u: U8): Bool\n"

    "primitive Foo\n"
    "  fun apply(t': (T1 | T2 | T3)): Bool =>\n"
    "    match t'\n"
    "    | let t: T1 => true\n"
    "    | let t: T2 => false\n"
    "    | let t: T3 if t > 0 => false\n"
    "    end";

  TEST_ERRORS_1(src, "function body isn't the result type");
}


TEST_F(SugarExprTest, MatchExhaustiveAllCasesPlusSomeCasesGuarded)
{
  const char* src =
    "trait T1\n"
    "trait T2 fun gt(u: U8): Bool\n"
    "trait T3 fun gt(u: U8): Bool\n"

    "primitive Foo\n"
    "  fun apply(t': (T1 | T2 | T3)): Bool =>\n"
    "    match t'\n"
    "    | let t: T1 => true\n"
    "    | let t: T2 if t > 0 => false\n"
    "    | let t: T2 => true\n"
    "    | let t: T3 if t > 0 => false\n"
    "    | let t: T3 => true\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(SugarExprTest, MatchNonExhaustiveSomeCasesEqMethod)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"
    "trait T3 fun eq(other: T3): Bool\n"

    "primitive Foo\n"
    "  fun apply(t': (T1 | T2 | T3), t3: T3): Bool =>\n"
    "    match t'\n"
    "    | let t: T1 => true\n"
    "    | let t: T2 => false\n"
    "    | t3 => false\n"
    "    end";

  TEST_ERRORS_1(src, "function body isn't the result type");
}


TEST_F(SugarExprTest, MatchExhaustiveAllCasesPlusSomeCasesEqMethod)
{
  const char* src =
    "trait T1\n"
    "trait T2 fun eq(other: T3): Bool\n"
    "trait T3 fun eq(other: T3): Bool\n"

    "primitive Foo\n"
    "  fun apply(t': (T1 | T2 | T3), t2: T2, t3: T3): Bool =>\n"
    "    match t'\n"
    "    | let t: T1 => true\n"
    "    | t2 => false\n"
    "    | let t: T2 => true\n"
    "    | t3 => false\n"
    "    | let t: T3 => true\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(SugarExprTest, MatchExhaustiveAllCasesIncludingDontCareAndTuple)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"
    "trait T3\n"

    "primitive Foo\n"
    "  fun apply(t': (T1 | T2 | (T3, Bool))): Bool =>\n"
    "    match t'\n"
    "    | let t: T1 => true\n"
    "    | let _: T2 => false\n"
    "    | (let t: T3, let _: Bool) => true\n"
    "    end";

  TEST_COMPILE(src);
}



TEST_F(SugarExprTest, MatchNonExhaustiveSomeTupleCaseElementsMatchOnValue)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"
    "trait T3\n"

    "primitive Foo\n"
    "  fun apply(t': (T1 | T2 | (T3, Bool))): Bool =>\n"
    "    match t'\n"
    "    | let t: T1 => true\n"
    "    | let _: T2 => false\n"
    "    | (let t: T3, true) => true\n"
    "    end";

  TEST_ERRORS_1(src, "function body isn't the result type");
}


TEST_F(SugarExprTest, MatchExhaustiveAllCasesPrimitiveValues)
{
  const char* src =
    "primitive P1\n"
    "primitive P2\n"
    "primitive P3\n"

    "primitive Foo\n"
    "  fun apply(t: (P1 | P2 | P3)): Bool =>\n"
    "    match t\n"
    "    | P1 | P2 => false\n"
    "    | P3 => true\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(SugarExprTest, MatchNonExhaustivePrimitiveValuesSomeCasesMissing)
{
  const char* src =
    "primitive P1\n"
    "primitive P2\n"
    "primitive P3\n"

    "primitive Foo\n"
    "  fun apply(p: (P1 | P2 | P3)): Bool =>\n"
    "    match p\n"
    "    | P1 => false\n"
    "    | P3 => true\n"
    "    end";

  TEST_ERRORS_1(src, "function body isn't the result type");
}


TEST_F(SugarExprTest, MatchNonExhaustivePrimitiveValuesMachineWords)
{
  const char* src =
    "primitive Foo\n"
    "  fun apply(n: (U64 | U32 | U8)): Bool =>\n"
    "    match n\n"
    "    | U64(0) => false\n"
    "    | U32(0) => true\n"
    "    | U8(0) => false\n"
    "    end";

  TEST_ERRORS_1(src, "function body isn't the result type");
}


TEST_F(SugarExprTest, MatchNonExhaustivePrimitiveValuesCustomEqMethod)
{
  const char* src =
    "primitive P1\n"
    "primitive P2\n"
    "primitive P3 fun eq(that: P3): Bool => this isnt that\n"

    "primitive Foo\n"
    "  fun apply(p: (P1 | P2 | P3)): Bool =>\n"
    "    match p\n"
    "    | P1 | P2 => false\n"
    "    | P3 => true\n"
    "    end";

  TEST_ERRORS_1(src, "function body isn't the result type");
}


TEST_F(SugarExprTest, MatchNonExhaustiveClassValues)
{
  const char* src =
    "class C1 fun eq(that: C1): Bool => this is that\n"
    "class C2 fun eq(that: C2): Bool => this is that\n"
    "class C3 fun eq(that: C3): Bool => this is that\n"

    "primitive Foo\n"
    "  fun apply(c: (C1 | C2 | C3)): Bool =>\n"
    "    match c\n"
    "    | C1 | C2 => false\n"
    "    | C3 => true\n"
    "    end";

  TEST_ERRORS_1(src, "function body isn't the result type");
}


TEST_F(SugarExprTest, MatchExhaustiveTupleIncludesDontCareType)
{
  const char* src =
    "primitive Foo\n"
    "  fun apply(t: (U32, U32)): Bool =>\n"
    "    match t\n"
    "    | (0, 0) => true\n"
    "    | (let c: U32, _) => c != 7\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(SugarExprTest, MatchNonExhaustiveAllCasesJumpAway)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"
    "trait T3\n"

    "primitive Foo\n"
    "  fun apply(t': (T1 | T2 | T3)): Bool =>\n"
    "    match t'\n"
    "    | let t: T1 => return true\n"
    "    | let t: T2 => return false\n"
    "    | let t: T3 => return true\n"
    "    end";

  TEST_ERRORS_1(src, "function body isn't the result type");
}


TEST_F(SugarExprTest, MatchExhaustiveSomeCasesJumpAway)
{
  const char* src =
    "trait T1\n"
    "trait T2\n"
    "trait T3\n"

    "primitive Foo\n"
    "  fun apply(t': (T1 | T2 | T3)): Bool =>\n"
    "    match t'\n"
    "    | let t: T1 => true\n"
    "    | let t: T2 => return false\n"
    "    | let t: T3 => return true\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(SugarExprTest, MatchExhaustiveSingleElementTuples)
{
  // From issue #1991
  const char* src =
    "primitive T1\n"
    "primitive T2\n"
    "primitive T3\n"

    "primitive Foo\n"
    "  fun ref apply(p': (T1 | T2 | T3)): Bool =>\n"
    "    match p'\n"
    "    | (let p: T1) => true\n"
    "    | (let p: (T2 | T3)) => false\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(SugarExprTest, As)
{
  const char* short_form =
    "class Bar\n"

    "class Foo\n"
    "  var create: U32\n"
    "  fun f(a: (Foo | Bar)): Foo ? =>\n"
    "    a as Foo ref";

  const char* full_form =
    "class Bar\n"

    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(a: (Foo | Bar)): Foo ? =>\n"
    "    match a\n"
    "    | $let $1: Foo ref => consume $aliased $1\n"
    "    else\n"
    "      error\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, AsTuple)
{
  const char* short_form =
    "class Bar\n"

    "class Foo\n"
    "  var create: U32\n"
    "  fun f(a: (Foo, Bar)): (Foo, Bar) ? =>\n"
    "    a as (Foo ref, Bar ref)";

  const char* full_form =
    "class Bar\n"

    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(a: (Foo, Bar)): (Foo, Bar) ? =>\n"
    "    match a\n"
    "    | ($let $1: Foo ref, $let $2: Bar ref) =>\n"
    "      (consume $aliased $1, consume $aliased $2)\n"
    "    else\n"
    "      error\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, AsNestedTuple)
{
  const char* short_form =
    "class Bar\n"
    "class Baz\n"

    "class Foo\n"
    "  var create: U32\n"
    "  fun f(a: (Foo, (Bar, Baz))): (Foo, (Bar, Baz)) ? =>\n"
    "    a as (Foo ref, (Bar ref, Baz ref))";

  const char* full_form =
    "class Bar\n"
    "class Baz\n"

    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(a: (Foo, (Bar, Baz))): (Foo, (Bar, Baz)) ? =>\n"
    "    match a\n"
    "    | ($let $1: Foo ref, ($let $2: Bar ref, $let $3: Baz ref)) =>\n"
    "      (consume $aliased $1,\n"
    "        (consume $aliased $2, consume $aliased $3))\n"
    "    else\n"
    "      error\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, AsDontCare)
{
  const char* short_form =
    "class Foo\n"
    "  fun f(a: Foo): Foo ? =>\n"
    "    a as (_)";

  TEST_ERRORS_1(short_form, " ");
}


TEST_F(SugarExprTest, AsDontCare2Tuple)
{
  const char* short_form =
    "class Bar\n"

    "class Foo\n"
    "  var create: U32\n"
    "  fun f(a: (Foo, Bar)): Foo ? =>\n"
    "    a as (Foo ref, _)";

  const char* full_form =
    "class Bar\n"

    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(a: (Foo, Bar)): Foo ? =>\n"
    "    match a\n"
    "    | ($let $1: Foo ref, _) =>\n"
    "      consume $aliased $1\n"
    "    else\n"
    "      error\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarExprTest, AsDontCareMultiTuple)
{
  const char* short_form =
    "class Bar\n"
    "class Baz\n"

    "class Foo\n"
    "  var create: U32\n"
    "  fun f(a: (Foo, Bar, Baz)): (Foo, Baz) ? =>\n"
    "    a as (Foo ref, _, Baz ref)";

  const char* full_form =
    "class Bar\n"
    "class Baz\n"

    "class ref Foo\n"
    "  var create: U32\n"
    "  fun box f(a: (Foo, Bar, Baz)): (Foo, Baz) ? =>\n"
    "    match a\n"
    "    | ($let $1: Foo ref, _, $let $2: Baz ref) =>\n"
    "      (consume $aliased $1, consume $aliased $2)\n"
    "    else\n"
    "      error\n"
    "    end";

  TEST_EQUIV(short_form, full_form);
}
