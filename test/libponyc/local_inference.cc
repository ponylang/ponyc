#include <gtest/gtest.h>
#include <platform.h>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_EQUIV(src, expect) DO(test_equiv(src, "expr", expect, "expr"))


class LocalInferTest: public PassTest
{};


TEST_F(LocalInferTest, Simple)
{
  const char* short_form =
    "class C\n"
    "  fun f() =>\n"
    "    var a = None";

  const char* full_form =
    "class C\n"
    "  fun f() =>\n"
    "    var a: None = None";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(LocalInferTest, Tuple)
{
  const char* short_form =
    "class C\n"
    "  fun f() =>\n"
    "    (var a, var b) = (None, true)";

  const char* full_form =
    "class C\n"
    "  fun f() =>\n"
    "    (var a: None, var b: Bool) = (None, true)";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(LocalInferTest, NestedTuple)
{
  const char* short_form =
    "class C\n"
    "  fun f() =>\n"
    "    (var a, (var b, var c)) = (None, (true, U32))";

  const char* full_form =
    "class C\n"
    "  fun f() =>\n"
    "    (var a: None, (var b: Bool, var c: U32)) = (None, (true, U32))";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(LocalInferTest, DontCare)
{
  const char* short_form =
    "class C\n"
    "  fun f() =>\n"
    "    (var a, (_, var c)) = (None, (true, U32))";

  const char* full_form =
    "class C\n"
    "  fun f() =>\n"
    "    (var a: None, (_, var c: U32)) = (None, (true, U32))";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(LocalInferTest, Union)
{
  const char* short_form =
    "class C\n"
    "  fun f(x:(None | U32)) =>\n"
    "    var a = x";

  const char* full_form =
    "class C\n"
    "  fun f(x:(None | U32)) =>\n"
    "    var a:(None | U32) = x";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(LocalInferTest, UnionOfTuples)
{
  const char* short_form =
    "class C\n"
    "  fun f(x:((None, U32) | (Bool, U32))) =>\n"
    "    (var a, var b) = x";

  TEST_ERROR(short_form);
}


TEST_F(LocalInferTest, VarAndLet)
{
  const char* short_form =
    "class C\n"
    "  fun f() =>\n"
    "    (var a, (let b, let c)) = (None, (true, U32))";

  const char* full_form =
    "class C\n"
    "  fun f() =>\n"
    "    (var a: None, (let b: Bool, let c: U32)) = (None, (true, U32))";

  TEST_EQUIV(short_form, full_form);
}


TEST_F(LocalInferTest, DeclAndNot)
{
  const char* short_form =
    "class C\n"
    "  fun f() =>\n"
    "    var b: Bool\n"
    "    (var a, (b, let c)) = (None, (true, U32))";

  const char* full_form =
    "class C\n"
    "  fun f() =>\n"
    "    var b: Bool\n"
    "    (var a: None, (b, let c: U32)) = (None, (true, U32))";

  TEST_EQUIV(short_form, full_form);
}
