#include <gtest/gtest.h>
#include "util.h"


// Parsing tests regarding expressions

#define TEST_ERROR(src) DO(test_error(src, "expr"))
#define TEST_COMPILE(src) DO(test_compile(src, "expr"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "expr", errs)); }


class LambdaTest : public PassTest
{};


// Lambda tests

TEST_F(LambdaTest, Lambda)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {() => None}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCap)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    {iso() => None}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCaptureVariable)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x = \"hi\"\n"
    "    {()(x): String => x}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCaptureExpression)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let y = \"hi\"\n"
    "    {()(x = y): String => x}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCaptureExpressionAndType)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let y = \"hi\"\n"
    "    {()(x: String = y): String => x}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCaptureTypeWithoutExpressionFail)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x = \"hi\"\n"
    "    {()(x: String): String => x}";

  TEST_ERROR(src);
}


TEST_F(LambdaTest, LambdaCaptureFree)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x = \"hi\"\n"
    "    {(): String => x}";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, LambdaCaptureRefInIso)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x: String ref = String\n"
    "    {(): String ref => x} iso";

  TEST_ERROR(src);
}


TEST_F(LambdaTest, LambdaCaptureRefInVal)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x: String ref = String\n"
    "    {(): String ref => x} val";

  TEST_ERROR(src);
}


TEST_F(LambdaTest, ObjectCaptureFree)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x = \"hi\"\n"
    "    object\n"
    "      fun apply(): String => x\n"
    "    end";

  TEST_COMPILE(src);
}


TEST_F(LambdaTest, ObjectCaptureRefInActor)
{
  const char* src =
    "class Foo\n"
    "  fun f() =>\n"
    "    let x: String ref = String\n"
    "    object\n"
    "      be apply(): String ref => x\n"
    "    end";

  TEST_ERROR(src);
}
