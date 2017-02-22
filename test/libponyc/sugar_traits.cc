#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"


// Tests for sugar that is applied during the traits pass

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_EQUIV(src, expect) DO(test_equiv(src, "traits", expect, "traits"))


class SugarTraitTest : public PassTest
{};


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
