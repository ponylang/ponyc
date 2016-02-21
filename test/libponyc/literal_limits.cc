#include <gtest/gtest.h>
#include "util.h"


// Literal limit checks

#define TEST_COMPILE(src) DO(test_compile(src, "expr"))
#define TEST_ERROR(src) DO(test_error(src, "expr"))


class LiteralLimitTest : public PassTest
{
};


TEST_F(LiteralLimitTest, U8Max)
{
  TEST_COMPILE("class Foo let m:U8 = 255");
  TEST_ERROR("class Foo let m:U8 = 256");
}


TEST_F(LiteralLimitTest, I8Max)
{
  TEST_COMPILE("class Foo let m:I8 = 127");
  TEST_ERROR("class Foo let m:I8 = 128");
}


TEST_F(LiteralLimitTest, I8Min)
{
  TEST_COMPILE("class Foo let m:I8 = -128");
  TEST_ERROR("class Foo let m:I8 = -129");
}


TEST_F(LiteralLimitTest, U16Max)
{
  TEST_COMPILE("class Foo let m:U16 = 65535");
  TEST_ERROR("class Foo let m:U16 = 65536");
}


TEST_F(LiteralLimitTest, I16Max)
{
  TEST_COMPILE("class Foo let m:I16 = 32767");
  TEST_ERROR("class Foo let m:I16 = 32768");
}


TEST_F(LiteralLimitTest, I16Min)
{
  TEST_COMPILE("class Foo let m:I16 = -32768");
  TEST_ERROR("class Foo let m:I16 = -32769");
}


TEST_F(LiteralLimitTest, U32Max)
{
  TEST_COMPILE("class Foo let m:U32 =  0xFFFFFFFF");
  TEST_ERROR("class Foo let m:U32 = 0x100000000");
}


TEST_F(LiteralLimitTest, I32Max)
{
  TEST_COMPILE("class Foo let m:I32 = 0x7FFFFFFF");
  TEST_ERROR("class Foo let m:I32 = 0x80000000");
}


TEST_F(LiteralLimitTest, I32Min)
{
  TEST_COMPILE("class Foo let m:I32 = -0x80000000");
  TEST_ERROR("class Foo let m:I32 = -0x80000001");
}


TEST_F(LiteralLimitTest, U64Max)
{
  TEST_COMPILE("class Foo let m:U64 =  0xFFFFFFFFFFFFFFFF");
  TEST_ERROR("class Foo let m:U64 = 0x10000000000000000");
}


TEST_F(LiteralLimitTest, I64Max)
{
  TEST_COMPILE("class Foo let m:I64 = 0x7FFFFFFFFFFFFFFF");
  TEST_ERROR("class Foo let m:I64 = 0x8000000000000000");
}


TEST_F(LiteralLimitTest, I64Min)
{
  TEST_COMPILE("class Foo let m:I64 = -0x8000000000000000");
  TEST_ERROR("class Foo let m:I64 = -0x8000000000000001");
}


TEST_F(LiteralLimitTest, U128Max)
{
  // This is checked by the lexer, including here for completeness
  TEST_COMPILE("class Foo let m:U128 =  0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  TEST_ERROR("class Foo let m:U128 = 0x100000000000000000000000000000000");
}


TEST_F(LiteralLimitTest, I128Max)
{
  TEST_COMPILE("class Foo let m:I128 = 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  TEST_ERROR("class Foo let m:I128 = 0x80000000000000000000000000000000");
}


TEST_F(LiteralLimitTest, I128Min)
{
  TEST_COMPILE("class Foo let m:I128 = -0x80000000000000000000000000000000");
  TEST_ERROR("class Foo let m:I128 = -0x80000000000000000000000000000001");
}
