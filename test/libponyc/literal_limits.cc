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
  ASSERT_EQ(0, get_error_count());
  TEST_COMPILE("class Foo let m:U8 = 256");
  ASSERT_EQ(1, get_error_count());
}


TEST_F(LiteralLimitTest, I8Max)
{
  TEST_COMPILE("class Foo let m:I8 = 127");
  ASSERT_EQ(0, get_error_count());
  TEST_COMPILE("class Foo let m:I8 = 128");
  ASSERT_EQ(1, get_error_count());
}


TEST_F(LiteralLimitTest, I8Min)
{
  TEST_COMPILE("class Foo let m:I8 = -128");
  ASSERT_EQ(0, get_error_count());
  TEST_COMPILE("class Foo let m:I8 = -129");
  ASSERT_EQ(1, get_error_count());
}


TEST_F(LiteralLimitTest, U16Max)
{
  TEST_COMPILE("class Foo let m:U16 = 65535");
  ASSERT_EQ(0, get_error_count());
  TEST_COMPILE("class Foo let m:U16 = 65536");
  ASSERT_EQ(1, get_error_count());
}


TEST_F(LiteralLimitTest, I16Max)
{
  TEST_COMPILE("class Foo let m:I16 = 32767");
  ASSERT_EQ(0, get_error_count());
  TEST_COMPILE("class Foo let m:I16 = 32768");
  ASSERT_EQ(1, get_error_count());
}


TEST_F(LiteralLimitTest, I16Min)
{
  TEST_COMPILE("class Foo let m:I16 = -32768");
  ASSERT_EQ(0, get_error_count());
  TEST_COMPILE("class Foo let m:I16 = -32769");
  ASSERT_EQ(1, get_error_count());
}


TEST_F(LiteralLimitTest, U32Max)
{
  TEST_COMPILE("class Foo let m:U32 =  0xFFFFFFFF");
  ASSERT_EQ(0, get_error_count());
  TEST_COMPILE("class Foo let m:U32 = 0x100000000");
  ASSERT_EQ(1, get_error_count());
}


TEST_F(LiteralLimitTest, I32Max)
{
  TEST_COMPILE("class Foo let m:I32 = 0x7FFFFFFF");
  ASSERT_EQ(0, get_error_count());
  TEST_COMPILE("class Foo let m:I32 = 0x80000000");
  ASSERT_EQ(1, get_error_count());
}


TEST_F(LiteralLimitTest, I32Min)
{
  TEST_COMPILE("class Foo let m:I32 = -0x80000000");
  ASSERT_EQ(0, get_error_count());
  TEST_COMPILE("class Foo let m:I32 = -0x80000001");
  ASSERT_EQ(1, get_error_count());
}


TEST_F(LiteralLimitTest, U64Max)
{
  TEST_COMPILE("class Foo let m:U64 =  0xFFFFFFFFFFFFFFFF");
  ASSERT_EQ(0, get_error_count());
  TEST_COMPILE("class Foo let m:U64 = 0x10000000000000000");
  ASSERT_EQ(1, get_error_count());
}


TEST_F(LiteralLimitTest, I64Max)
{
  TEST_COMPILE("class Foo let m:I64 = 0x7FFFFFFFFFFFFFFF");
  ASSERT_EQ(0, get_error_count());
  TEST_COMPILE("class Foo let m:I64 = 0x8000000000000000");
  ASSERT_EQ(1, get_error_count());
}


TEST_F(LiteralLimitTest, I64Min)
{
  TEST_COMPILE("class Foo let m:I64 = -0x8000000000000000");
  ASSERT_EQ(0, get_error_count());
  TEST_COMPILE("class Foo let m:I64 = -0x8000000000000001");
  ASSERT_EQ(1, get_error_count());
}


TEST_F(LiteralLimitTest, U128Max)
{
  // This is checked by the lexer, including here for completeness
  TEST_COMPILE("class Foo let m:U128 =  0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  ASSERT_EQ(0, get_error_count());
  TEST_ERROR("class Foo let m:U128 = 0x100000000000000000000000000000000");
}


TEST_F(LiteralLimitTest, I128Max)
{
  TEST_COMPILE("class Foo let m:I128 = 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  ASSERT_EQ(0, get_error_count());
  TEST_COMPILE("class Foo let m:I128 = 0x80000000000000000000000000000000");
  ASSERT_EQ(1, get_error_count());
}


TEST_F(LiteralLimitTest, I128Min)
{
  TEST_COMPILE("class Foo let m:I128 = -0x80000000000000000000000000000000");
  ASSERT_EQ(0, get_error_count());
  TEST_COMPILE("class Foo let m:I128 = -0x80000000000000000000000000000001");
  ASSERT_EQ(1, get_error_count());
}



