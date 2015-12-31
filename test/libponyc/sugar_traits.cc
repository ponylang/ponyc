#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


// Tests for sugar that is applied during the traits pass

#define TEST_COMPILE(src) DO(test_compile(src, "traits"))
#define TEST_ERROR(src) DO(test_error(src, "traits"))
#define TEST_EQUIV(src, expect) DO(test_equiv(src, "traits", expect, "traits"))


class SugarTraitTest : public PassTest
{};


TEST_F(SugarTraitTest, FieldDelegate)
{
  const char* short_form =
    "trait T\n"
    "  fun f()\n"

    "class Foo is T\n"
    "  var create: U32\n"
    "  var bar: T delegate T";

  const char* full_form =
    "trait T\n"
    "  fun f()\n"

    "class Foo is T\n"
    "  var create: U32\n"
    "  var bar: T delegate T\n"
    "  fun f() => bar.f()";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTraitTest, FieldDelegateTypeCannotBeConrete)
{
  const char* short_form =
    "class T\n"
    "  fun f()\n"

    "class Foo is T\n"
    "  var bar: T delegate T";

  TEST_ERROR(short_form);
}


TEST_F(SugarTraitTest, FieldDelegateTypeCannotBeUnimplementedInterface)
{
  const char* short_form =
    "interface T\n"
    "  fun f()\n"

    "class Foo\n"
    "  var bar: T delegate T";

  TEST_ERROR(short_form);
}


TEST_F(SugarTraitTest, FieldDelegateTypeCannotBeUnimplementedTrait)
{
  const char* short_form =
    "trait T\n"
    "  fun f()\n"

    "class Foo\n"
    "  var bar: T delegate T";

  TEST_ERROR(short_form);
}


TEST_F(SugarTraitTest, FieldDelegateParamsReturn)
{
  const char* short_form =
    "trait T\n"
    "  fun f(x: U32, y: U32): Bool\n"

    "class Foo is T\n"
    "  var create: U32\n"
    "  var bar: T delegate T";

  const char* full_form =
    "trait T\n"
    "  fun f(x: U32, y: U32): Bool\n"

    "class Foo is T\n"
    "  var create: U32\n"
    "  var bar: T delegate T\n"
    "  fun f(x: U32, y: U32): Bool => bar.f(consume x, consume y)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTraitTest, FieldDelegateMultipleMethods)
{
  const char* short_form =
    "trait T\n"
    "  fun f1()\n"
    "  fun f2(x: U32, y: U32): Bool\n"
    "  fun f3(x: T)\n"

    "class Foo is T\n"
    "  var create: U32\n"
    "  var bar: T delegate T";

  const char* full_form =
    "trait T\n"
    "  fun f1()\n"
    "  fun f2(x: U32, y: U32): Bool\n"
    "  fun f3(x: T)\n"

    "class Foo is T\n"
    "  var create: U32\n"
    "  var bar: T delegate T\n"
    "  fun f1() => bar.f1()\n"
    "  fun f2(x: U32, y: U32): Bool => bar.f2(consume x, consume y)\n"
    "  fun f3(x: T) => bar.f3(consume x)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTraitTest, FieldDelegateMaskedMethods)
{
  const char* short_form =
    "trait T\n"
    "  fun f1()\n"
    "  fun f2(x: U32, y: U32): Bool\n"
    "  fun f3(x: T)\n"

    "class Foo is T\n"
    "  var create: U32\n"
    "  var bar: T delegate T\n"
    "  fun f2(x: U32, y: U32): Bool => true\n"
    "  fun f3(x: T) => None";

  const char* full_form =
    "trait T\n"
    "  fun f1()\n"
    "  fun f2(x: U32, y: U32): Bool\n"
    "  fun f3(x: T)\n"

    "class Foo is T\n"
    "  var create: U32\n"
    "  var bar: T delegate T\n"
    "  fun f2(x: U32, y: U32): Bool => true\n"
    "  fun f3(x: T) => None\n"
    "  fun f1() => bar.f1()";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTraitTest, FieldDelegateDuplicateMethodsAddedOnce)
{
  const char* short_form =
    "trait T\n"
    "  fun f1()\n"
    "  fun f2(x: U32, y: U32): Bool\n"

    "interface I\n"
    "  fun f3()\n"
    "  fun f2(x: U32, y: U32): Bool\n"

    "class Foo is (T & I)\n"
    "  var create: U32\n"
    "  var bar: (T & I) delegate (T & I)";

  const char* full_form =
    "trait T\n"
    "  fun f1()\n"
    "  fun f2(x: U32, y: U32): Bool\n"

    "interface I\n"
    "  fun f3()\n"
    "  fun f2(x: U32, y: U32): Bool\n"

    "class Foo is (T & I)\n"
    "  var create: U32\n"
    "  var bar: (T & I) delegate (T & I)\n"
    "  fun f1() => bar.f1()\n"
    "  fun f2(x: U32, y: U32): Bool => bar.f2(consume x, consume y)\n"
    "  fun f3() => bar.f3()";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTraitTest, FieldDelegateClashingMethodsFail)
{
  const char* short_form =
    "trait T\n"
    "  fun f()\n"

    "interface I\n"
    "  fun f(x: U32)\n"

    "class Foo is T\n"
    "  var bar: T delegate (T & I)";

  TEST_ERROR(short_form);
}


TEST_F(SugarTraitTest, FieldDelegateMultipleDelegations)
{
  const char* short_form =
    "trait T1\n"
    "  fun f() => None\n"

    "trait T2\n"
    "  fun g() => None\n"

    "class Foo is (T1 & T2)\n"
    "  var create: U32\n"
    "  var bar1: T1 delegate T1\n"
    "  var bar2: T2 delegate T2";

  const char* full_form =
    "trait T1\n"
    "  fun f() => None\n"

    "trait T2\n"
    "  fun g() => None\n"

    "class Foo is (T1 & T2)\n"
    "  var create: U32\n"
    "  var bar1: T1 delegate T1\n"
    "  var bar2: T2 delegate T2\n"
    "  fun f() => bar1.f()\n"
    "  fun g() => bar2.g()";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTraitTest, FieldDelegateClashingDelegationsFail)
{
  const char* short_form =
    "trait T1\n"
    "  fun f() => None\n"

    "trait T2\n"
    "  fun f() => None\n"

    "class Foo is (T1 & T2)\n"
    "  var bar1: T1 delegate T1\n"
    "  var bar2: T2 delegate T2";

  TEST_ERROR(short_form);
}


TEST_F(SugarTraitTest, FieldDelegatePolymorphicTrait)
{
  const char* short_form =
    "trait T[A]\n"
    "  fun f()\n"

    "class Foo is T[U32]\n"
    "  var create: U32\n"
    "  var bar: T[U32] delegate T[U32]";

  const char* full_form =
    "trait T[A]\n"
    "  fun f()\n"

    "class Foo is T[U32]\n"
    "  var create: U32\n"
    "  var bar: T[U32] delegate T[U32]\n"
    "  fun f() => bar.f()";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTraitTest, FieldDelegatePolymorphicTraitMustMatch)
{
  const char* short_form =
    "trait T[A]\n"
    "  fun f()\n"

    "class Foo is T[U32]\n"
    "  var bar: T delegate T[U16]";

  TEST_ERROR(short_form);
}


TEST_F(SugarTraitTest, FieldDelegatePolymorphicMethod)
{
  const char* short_form =
    "trait T\n"
    "  fun f[A]()\n"

    "class Foo is T\n"
    "  var create: U32\n"
    "  var bar: T delegate T";

  const char* full_form =
    "trait T\n"
    "  fun f[A]()\n"

    "class Foo is T\n"
    "  var create: U32\n"
    "  var bar: T delegate T\n"
    "  fun f[A]() => bar.f()";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTraitTest, PrimitiveEqAdded)
{
  const char* short_form =
    "primitive Foo";

  const char* full_form =
    "primitive Foo\n"
    "  new create() => true\n"
    "  fun eq(that: Foo): Bool => this is that\n"
    "  fun ne(that: Foo): Bool => this isnt that";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTraitTest, PrimitiveEqAddedWithTypeArgs)
{
  const char* short_form =
    "trait T\n"

    "primitive Foo[A, B: T]";

  const char* full_form =
    "trait T\n"

    "primitive Foo[A, B: T]\n"
    "  new create() => true\n"
    "  fun eq(that: Foo[A, B]): Bool => this is that\n"
    "  fun ne(that: Foo[A, B]): Bool => this isnt that";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTraitTest, PrimitiveEqNotAddedIfAlreadyPresent)
{
  const char* short_form =
    "primitive Foo\n"
    "  fun eq() => None";

  const char* full_form =
    "primitive Foo\n"
    "  fun eq() => None\n"
    "  new create() => true\n"
    "  fun ne(that: Foo): Bool => this isnt that";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTraitTest, PrimitiveNeNotAddedIfAlreadyPresent)
{
  const char* short_form =
    "primitive Foo\n"
    "  fun ne() => None";

  const char* full_form =
    "primitive Foo\n"
    "  fun ne() => None\n"
    "  new create() => true\n"
    "  fun eq(that: Foo): Bool => this is that";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(SugarTraitTest, PrimitiveEqNotAddedIfInheritted)
{
  const char* short_form =
    "trait T\n"
    "  fun eq() => None\n"

    "primitive Foo is T";

  const char* full_form =
    "trait T\n"
    "  fun eq() => None\n"

    "primitive Foo is T\n"
    "  new create() => true\n"
    "  fun eq() => None\n"
    "  fun ne(that: Foo): Bool => this isnt that";

  TEST_EQUIV(short_form, full_form);
}
