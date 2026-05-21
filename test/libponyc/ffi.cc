#include <gtest/gtest.h>
#include "util.h"


// FFI type checking tests

#define TEST_ERROR(src) DO(test_error(src, "expr"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }

#define TEST_ERRORS_2(src, err1, err2) \
  { const char* errs[] = {err1, err2, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))


class FFITest : public PassTest
{};

TEST_F(FFITest, NoDeclaration)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    @foo()";

  TEST_ERROR(src);
}



TEST_F(FFITest, Declaration)
{
  const char* src =
    "use @foo[U32]()\n"

    "class Foo\n"
    "  fun f() =>\n"
    "    @foo()";

  TEST_COMPILE(src);
}

TEST_F(FFITest, DeclarationMultipleReturn)
{
  const char* src =
    "use @foo[Bool, U32]()\n"

    "class Foo\n"
    "  fun f() =>\n"
    "    @foo()";

  TEST_ERRORS_1(src, "FFI functions must specify a single return type");
}

TEST_F(FFITest, DeclarationMultipleReturnAtCallSite)
{
  const char* src =
    "use @foo[Bool]()\n"

    "class Foo\n"
    "  fun f() =>\n"
    "    @foo[Bool, U32]()";

  TEST_ERRORS_1(src, "FFI functions must specify a single return type");
}

TEST_F(FFITest, DeclarationArgs)
{
  const char* src =
    "use @foo[U32](a: Bool, b: U32)\n"

    "class Foo\n"
    "  fun f(x: U32) =>\n"
    "    @foo(true, x)";

  TEST_COMPILE(src);
}


TEST_F(FFITest, DeclarationBadArg)
{
  const char* src =
    "use @foo[U32](a: Bool, b: U32)\n"

    "class Foo\n"
    "  fun f(x: U32) =>\n"
    "    @foo(true, false)";

  TEST_ERROR(src);
}


TEST_F(FFITest, DeclarationTooFewArgs)
{
  const char* src =
    "use @foo[U32](a: Bool, b: U32)\n"

    "class Foo\n"
    "  fun f(x: U32) =>\n"
    "    @foo(true)";

  TEST_ERROR(src);
}


TEST_F(FFITest, DeclarationTooManyArgs)
{
  const char* src =
    "use @foo[U32](a: Bool, b: U32)\n"

    "class Foo\n"
    "  fun f(x: U32) =>\n"
    "    @foo(true, x, false)";

  TEST_ERROR(src);
}



TEST_F(FFITest, DeclarationVoidStarPointer)
{
  const char* src =
    "use @foo[U32](a: Pointer[None])\n"

    "class Foo\n"
    "  fun f(x: Pointer[U32]) =>\n"
    "    @foo(x)";

  TEST_COMPILE(src);
}


TEST_F(FFITest, DeclarationVoidStarUSize)
{
  const char* src =
    "use @foo[U32](a: Pointer[None])\n"

    "class Foo\n"
    "  fun f(x: USize) =>\n"
    "    @foo(x)";

  TEST_COMPILE(src);
}


TEST_F(FFITest, DeclarationVoidStarNotPointer)
{
  const char* src =
    "use @foo[U32](a: Pointer[None])\n"

    "class Foo\n"
    "  fun f(x: U8) =>\n"
    "    @foo(x)";

  TEST_ERROR(src);
}


TEST_F(FFITest, DeclarationEllipsisNoArgs)
{
  const char* src =
    "use @foo[U32](...)\n"

    "class Foo\n"
    "  fun f(x: U32) =>\n"
    "    @foo()";

  TEST_COMPILE(src);
}


TEST_F(FFITest, DeclarationEllipsisOneArg)
{
  const char* src =
    "use @foo[U32](...)\n"

    "class Foo\n"
    "  fun f(x: U32) =>\n"
    "    @foo(true)";

  TEST_COMPILE(src);
}


TEST_F(FFITest, DeclarationEllipsisMultipleArgs)
{
  const char* src =
    "use @foo[U32](...)\n"

    "class Foo\n"
    "  fun f(x: U32) =>\n"
    "    @foo(true, x)";

  TEST_COMPILE(src);
}


TEST_F(FFITest, DeclarationEllipsisNotFirst)
{
  const char* src =
    "use @foo[U32](a: Bool, ...)\n"

    "class Foo\n"
    "  fun f(x: U32) =>\n"
    "    @foo(true, x)";

  TEST_COMPILE(src);
}


TEST_F(FFITest, DeclarationEllipsisNotLast)
{
  const char* src =
    "use @foo[U32](..., a: Bool)";

  TEST_ERROR(src);
}


TEST_F(FFITest, DeclarationAlias)
{
  const char* src =
    "use @foo[None](x: Foo iso)\n"

    "class Foo\n"
    "  fun f() =>\n"
    "    let x: Foo iso = Foo\n"
    "    @foo(x)";

  TEST_ERROR(src);
}

TEST_F(FFITest, DeclarationWithNullablePointer)
{
  // From issue #3757
  const char* src =
    "use @foo[NullablePointer[(U64, U64)]]()\n"
    "use @bar[NullablePointer[P1]]()\n"
    "primitive P1\n"

    "actor Main\n"
    " new create(env: Env) =>\n"
    "   None";

    TEST_ERRORS_2(src,
      "NullablePointer[(U64 val, U64 val)] ref is not allowed: "
        "the type argument to NullablePointer must be a struct",

      "NullablePointer[P1 val] ref is not allowed: "
        "the type argument to NullablePointer must be a struct");

}

TEST_F(FFITest, DeclarationCannotBePartial)
{
  const char* src =
    "use @foo[U64]() ?\n"

    "primitive Foo\n"
    "  fun apply(): U64 =>\n"
    "    @foo()";

  TEST_ERRORS_1(src, "an FFI declaration can no longer be partial");
}

TEST_F(FFITest, CallCannotBePartial)
{
  const char* src =
    "use @foo[U64]()\n"

    "primitive Foo\n"
    "  fun apply(): U64 =>\n"
    "    @foo() ?";

  TEST_ERRORS_1(src, "an FFI call can no longer be partial");
}

TEST_F(FFITest, DeclarationAndCallCannotBePartial)
{
  const char* src =
    "use @foo[U64]() ?\n"

    "primitive Foo\n"
    "  fun apply(): U64 =>\n"
    "    @foo()?";

  TEST_ERRORS_2(src,
    "an FFI declaration can no longer be partial",
    "an FFI call can no longer be partial");
}

// A `?` on a method call on an FFI's result (here S.get) binds to that call,
// not the FFI call, so it remains legal.
TEST_F(FFITest, CallOnFFIResultMayBePartial)
{
  const char* src =
    "use @foo[S]()\n"

    "struct S\n"
    "  fun get(): U8 ? => error\n"

    "primitive Foo\n"
    "  fun apply(): U8 ? =>\n"
    "    @foo().get()?";

  TEST_COMPILE(src);
}
