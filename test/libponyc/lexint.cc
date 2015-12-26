#include <gtest/gtest.h>
#include <platform.h>
#include <string>

#include <ast/lexint.h>

#include "util.h"


class LexIntTest : public testing::Test
{};


static lexint_t* lexint(lexint_t* i, uint64_t high, uint64_t low)
{
  assert(i != NULL);
  i->low = low;
  i->high = high;
  return i;
}

#define ASSERT_LIEQ(lexint, h, l) \
  { \
    ASSERT_EQ(l, lexint.low); \
    ASSERT_EQ(h, lexint.high); \
  }


TEST_F(LexIntTest, LexIntCmp)
{
  lexint_t a, b;

  ASSERT_EQ(0, lexint_cmp(lexint(&a, 0, 0), lexint(&b, 0, 0)));
  ASSERT_EQ(0, lexint_cmp(lexint(&a, 0, 1), lexint(&b, 0, 1)));
  ASSERT_EQ(0, lexint_cmp(lexint(&a, 2, 0), lexint(&b, 2, 0)));
  ASSERT_EQ(0, lexint_cmp(lexint(&a, 2, 1), lexint(&b, 2, 1)));

  ASSERT_EQ(1, lexint_cmp(lexint(&a, 0, 1), lexint(&b, 0, 0)));
  ASSERT_EQ(1, lexint_cmp(lexint(&a, 1, 0), lexint(&b, 0, 0)));
  ASSERT_EQ(1, lexint_cmp(lexint(&a, 0, 2), lexint(&b, 0, 1)));
  ASSERT_EQ(1, lexint_cmp(lexint(&a, 1, 0), lexint(&b, 0, 1)));
  ASSERT_EQ(1, lexint_cmp(lexint(&a, 1, 1), lexint(&b, 1, 0)));

  ASSERT_EQ(-1, lexint_cmp(lexint(&a, 0, 0), lexint(&b, 0, 1)));
  ASSERT_EQ(-1, lexint_cmp(lexint(&a, 0, 0), lexint(&b, 1, 0)));
  ASSERT_EQ(-1, lexint_cmp(lexint(&a, 0, 1), lexint(&b, 0, 2)));
  ASSERT_EQ(-1, lexint_cmp(lexint(&a, 0, 1), lexint(&b, 1, 0)));
  ASSERT_EQ(-1, lexint_cmp(lexint(&a, 1, 0), lexint(&b, 1, 1)));
}


TEST_F(LexIntTest, LexIntCmp64)
{
  lexint_t a;

  ASSERT_EQ(0, lexint_cmp64(lexint(&a, 0, 0), 0));
  ASSERT_EQ(0, lexint_cmp64(lexint(&a, 0, 1), 1));

  ASSERT_EQ(1, lexint_cmp64(lexint(&a, 0, 1), 0));
  ASSERT_EQ(1, lexint_cmp64(lexint(&a, 1, 0), 0));
  ASSERT_EQ(1, lexint_cmp64(lexint(&a, 0, 2), 1));
  ASSERT_EQ(1, lexint_cmp64(lexint(&a, 1, 0), 1));

  ASSERT_EQ(-1, lexint_cmp64(lexint(&a, 0, 0), 1));
  ASSERT_EQ(-1, lexint_cmp64(lexint(&a, 0, 1), 2));
}


TEST_F(LexIntTest, LexIntShiftLeft)
{
  lexint_t a, dst;

  lexint_shl(&dst, lexint(&a, 0, 0), 0);
  ASSERT_LIEQ(dst, 0, 0);

  lexint_shl(&dst, lexint(&a, 0, 0), 4);
  ASSERT_LIEQ(dst, 0, 0);

  lexint_shl(&dst, lexint(&a, 0, 1), 4);
  ASSERT_LIEQ(dst, 0, 16);

  lexint_shl(&dst, lexint(&a, 0, 0x1122334455667788), 16);
  ASSERT_LIEQ(dst, 0x1122, 0x3344556677880000);

  lexint_shl(&dst, lexint(&a, 0x8877665544332211, 0x1122334455667788), 16);
  ASSERT_LIEQ(dst, 0x6655443322111122, 0x3344556677880000);

  lexint_shl(&dst, lexint(&a, 0, 0x1122334455667788), 64);
  ASSERT_LIEQ(dst, 0x1122334455667788, 0);

  lexint_shl(&dst, lexint(&a, 0, 0x1122334455667788), 72);
  ASSERT_LIEQ(dst, 0x2233445566778800, 0);

  lexint_shl(&dst, lexint(&a, 0x1122334455667788, 0x1122334455667788), 128);
  ASSERT_LIEQ(dst, 0, 0);
}


TEST_F(LexIntTest, LexIntShiftRight)
{
  lexint_t a, dst;

  lexint_shr(&dst, lexint(&a, 0, 0), 0);
  ASSERT_LIEQ(dst, 0, 0);

  lexint_shr(&dst, lexint(&a, 0, 0), 4);
  ASSERT_LIEQ(dst, 0, 0);

  lexint_shr(&dst, lexint(&a, 0, 16), 4);
  ASSERT_LIEQ(dst, 0, 1);

  lexint_shr(&dst, lexint(&a, 0, 0x1122334455667788), 16);
  ASSERT_LIEQ(dst, 0, 0x112233445566);

  lexint_shr(&dst, lexint(&a, 0x1122334455667788, 0), 16);
  ASSERT_LIEQ(dst, 0x112233445566, 0x7788000000000000);

  lexint_shr(&dst, lexint(&a, 0x8877665544332211, 0x1122334455667788), 16);
  ASSERT_LIEQ(dst, 0x887766554433, 0x2211112233445566);

  lexint_shr(&dst, lexint(&a, 0x1122334455667788, 0), 64);
  ASSERT_LIEQ(dst, 0, 0x1122334455667788);

  lexint_shr(&dst, lexint(&a, 0x1122334455667788, 0), 72);
  ASSERT_LIEQ(dst, 0, 0x11223344556677);

  lexint_shr(&dst, lexint(&a, 0x1122334455667788, 0x1122334455667788), 128);
  ASSERT_LIEQ(dst, 0, 0);
}


TEST_F(LexIntTest, LexIntTestBit)
{
  lexint_t a;

  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0, 0), 4));

  ASSERT_EQ(1, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 0));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 1));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 2));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 3));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 60));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 61));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 62));
  ASSERT_EQ(1, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 63));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 64));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 65));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 66));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 67));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 124));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 125));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 126));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0, 0x8000000000000001), 127));

  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 0));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 1));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 2));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 3));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 60));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 61));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 62));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 63));
  ASSERT_EQ(1, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 64));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 65));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 66));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 67));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 124));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 125));
  ASSERT_EQ(0, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 126));
  ASSERT_EQ(1, lexint_testbit(lexint(&a, 0x8000000000000001, 0), 127));
}


TEST_F(LexIntTest, LexIntSetBit)
{
  lexint_t a, dst;

  lexint_setbit(&dst, lexint(&a, 0, 0), 0);
  ASSERT_LIEQ(dst, 0, 1);

  lexint_setbit(&dst, lexint(&a, 0, 1), 0);
  ASSERT_LIEQ(dst, 0, 1);

  lexint_setbit(&dst, lexint(&a, 0, 0), 1);
  ASSERT_LIEQ(dst, 0, 2);

  lexint_setbit(&dst, lexint(&a, 0, 0), 63);
  ASSERT_LIEQ(dst, 0, 0x8000000000000000);

  lexint_setbit(&dst, lexint(&a, 0, 0), 64);
  ASSERT_LIEQ(dst, 1, 0);

  lexint_setbit(&dst, lexint(&a, 0, 0), 65);
  ASSERT_LIEQ(dst, 2, 0);

  lexint_setbit(&dst, lexint(&a, 0, 0), 127);
  ASSERT_LIEQ(dst, 0x8000000000000000, 0);

  lexint_setbit(&dst, lexint(&a, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF), 37);
  ASSERT_LIEQ(dst, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF);
}


TEST_F(LexIntTest, LexIntAdd)
{
  lexint_t a, b, dst;

  lexint_add(&dst, lexint(&a, 0, 0), lexint(&b, 0, 0));
  ASSERT_LIEQ(dst, 0, 0);

  lexint_add(&dst, lexint(&a, 0, 1), lexint(&b, 0, 0));
  ASSERT_LIEQ(dst, 0, 1);

  lexint_add(&dst, lexint(&a, 0, 0), lexint(&b, 0, 1));
  ASSERT_LIEQ(dst, 0, 1);

  lexint_add(&dst, lexint(&a, 2, 0), lexint(&b, 0, 0));
  ASSERT_LIEQ(dst, 2, 0);

  lexint_add(&dst, lexint(&a, 0, 0), lexint(&b, 3, 0));
  ASSERT_LIEQ(dst, 3, 0);

  lexint_add(&dst, lexint(&a, 1, 2), lexint(&b, 3, 4));
  ASSERT_LIEQ(dst, 4, 6);

  lexint_add(&dst, lexint(&a, 0, 0xFFFFFFFFFFFFFFFF), lexint(&b, 0, 2));
  ASSERT_LIEQ(dst, 1, 1);

  lexint_add(&dst, lexint(&a, 37, 0xFFFFFFFFFFFFFFFF), lexint(&b, 4, 7));
  ASSERT_LIEQ(dst, 42, 6);

  lexint_add(&dst, lexint(&a, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF),
    lexint(&b, 0, 1));
  ASSERT_LIEQ(dst, 0, 0);

  lexint_add(&dst, lexint(&a, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF),
    lexint(&b, 0, 4));
  ASSERT_LIEQ(dst, 0, 3)
}


TEST_F(LexIntTest, LexIntAdd64)
{
  lexint_t a, dst;

  lexint_add64(&dst, lexint(&a, 0, 0), 0);
  ASSERT_LIEQ(dst, 0, 0);

  lexint_add64(&dst, lexint(&a, 0, 1), 0);
  ASSERT_LIEQ(dst, 0, 1);

  lexint_add64(&dst, lexint(&a, 0, 0), 1);
  ASSERT_LIEQ(dst, 0, 1);

  lexint_add64(&dst, lexint(&a, 2, 0), 0);
  ASSERT_LIEQ(dst, 2, 0);

  lexint_add64(&dst, lexint(&a, 1, 2), 3);
  ASSERT_LIEQ(dst, 1, 5);

  lexint_add64(&dst, lexint(&a, 0, 0xFFFFFFFFFFFFFFFF), 2);
  ASSERT_LIEQ(dst, 1, 1);

  lexint_add64(&dst, lexint(&a, 37, 0xFFFFFFFFFFFFFFFF), 7);
  ASSERT_LIEQ(dst, 38, 6);

  lexint_add64(&dst, lexint(&a, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF), 1);
  ASSERT_LIEQ(dst, 0, 0);

  lexint_add64(&dst, lexint(&a, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF), 4);
  ASSERT_LIEQ(dst, 0, 3)
}


TEST_F(LexIntTest, LexIntSub)
{
  lexint_t a, b, dst;

  lexint_sub(&dst, lexint(&a, 0, 0), lexint(&b, 0, 0));
  ASSERT_LIEQ(dst, 0, 0);

  lexint_sub(&dst, lexint(&a, 0, 1), lexint(&b, 0, 0));
  ASSERT_LIEQ(dst, 0, 1);

  lexint_sub(&dst, lexint(&a, 2, 0), lexint(&b, 0, 0));
  ASSERT_LIEQ(dst, 2, 0);

  lexint_sub(&dst, lexint(&a, 4, 3), lexint(&b, 1, 2));
  ASSERT_LIEQ(dst, 3, 1);

  lexint_sub(&dst, lexint(&a, 1, 1), lexint(&b, 0, 2));
  ASSERT_LIEQ(dst, 0, 0xFFFFFFFFFFFFFFFF);

  lexint_sub(&dst, lexint(&a, 42, 3), lexint(&b, 4, 7));
  ASSERT_LIEQ(dst, 37, 0xFFFFFFFFFFFFFFFC);

  lexint_sub(&dst, lexint(&a, 0, 0), lexint(&b, 0, 1));
  ASSERT_LIEQ(dst, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF);

  lexint_sub(&dst, lexint(&a, 0, 0), lexint(&b, 3, 0));
  ASSERT_LIEQ(dst, 0xFFFFFFFFFFFFFFFD, 0);
}


TEST_F(LexIntTest, LexIntSub64)
{
  lexint_t a, dst;

  lexint_sub64(&dst, lexint(&a, 0, 0), 0);
  ASSERT_LIEQ(dst, 0, 0);

  lexint_sub64(&dst, lexint(&a, 0, 1), 0);
  ASSERT_LIEQ(dst, 0, 1);

  lexint_sub64(&dst, lexint(&a, 2, 0), 0);
  ASSERT_LIEQ(dst, 2, 0);

  lexint_sub64(&dst, lexint(&a, 4, 3), 2);
  ASSERT_LIEQ(dst, 4, 1);

  lexint_sub64(&dst, lexint(&a, 1, 1), 2);
  ASSERT_LIEQ(dst, 0, 0xFFFFFFFFFFFFFFFF);

  lexint_sub64(&dst, lexint(&a, 38, 3), 7);
  ASSERT_LIEQ(dst, 37, 0xFFFFFFFFFFFFFFFC);

  lexint_sub64(&dst, lexint(&a, 0, 0), 1);
  ASSERT_LIEQ(dst, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF);

  lexint_sub64(&dst, lexint(&a, 0, 0), 0xFFFFFFFFFFFFFFFF);
  ASSERT_LIEQ(dst, 0xFFFFFFFFFFFFFFFF, 1);
}


TEST_F(LexIntTest, LexIntMul64)
{
  lexint_t a, dst;

  lexint_mul64(&dst, lexint(&a, 0, 0), 0);
  ASSERT_LIEQ(dst, 0, 0);

  lexint_mul64(&dst, lexint(&a, 0, 1), 0);
  ASSERT_LIEQ(dst, 0, 0);

  lexint_mul64(&dst, lexint(&a, 0, 0), 1);
  ASSERT_LIEQ(dst, 0, 0);

  lexint_mul64(&dst, lexint(&a, 12, 34), 1);
  ASSERT_LIEQ(dst, 12, 34);

  lexint_mul64(&dst, lexint(&a, 12, 34), 2);
  ASSERT_LIEQ(dst, 24, 68);

  lexint_mul64(&dst, lexint(&a, 12, 34), 3);
  ASSERT_LIEQ(dst, 36, 102);

  lexint_mul64(&dst, lexint(&a, 0, 0x100000000), 0x100000000);
  ASSERT_LIEQ(dst, 1, 0);

  lexint_mul64(&dst, lexint(&a, 1, 0x100000000), 0x100000000);
  ASSERT_LIEQ(dst, 0x100000001, 0);

  lexint_mul64(&dst, lexint(&a, 0x100000000, 0x100000000), 0x100000000);
  ASSERT_LIEQ(dst, 1, 0);

  lexint_mul64(&dst, lexint(&a, 0x1122334455667788, 0x1122334455667788),
    0x1000);
  ASSERT_LIEQ(dst, 0x2334455667788112, 0x2334455667788000);
}


TEST_F(LexIntTest, LexIntDiv64)
{
  lexint_t a, dst;

  lexint_div64(&dst, lexint(&a, 0, 0), 1);
  ASSERT_LIEQ(dst, 0, 0);

  lexint_div64(&dst, lexint(&a, 36, 102), 1);
  ASSERT_LIEQ(dst, 36, 102);

  lexint_div64(&dst, lexint(&a, 36, 102), 2);
  ASSERT_LIEQ(dst, 18, 51);

  lexint_div64(&dst, lexint(&a, 36, 102), 3);
  ASSERT_LIEQ(dst, 12, 34);

  lexint_div64(&dst, lexint(&a, 1, 0), 0x100000000);
  ASSERT_LIEQ(dst, 0, 0x100000000);

  lexint_div64(&dst, lexint(&a, 0x100000001, 0), 0x100000000);
  ASSERT_LIEQ(dst, 1, 0x100000000);

  lexint_div64(&dst, lexint(&a, 0x2334455667788112, 0x2334455667788000),
    0x1000);
  ASSERT_LIEQ(dst, 0x2334455667788, 0x1122334455667788);

  lexint_mul64(&dst, lexint(&a, 0, 1), 0);
  ASSERT_LIEQ(dst, 0, 0);

  lexint_mul64(&dst, lexint(&a, 0, 0), 0);
  ASSERT_LIEQ(dst, 0, 0);
}


TEST_F(LexIntTest, LexIntChar)
{
  lexint_t a;
  lexint(&a, 0, 0);

  lexint_char(&a, 'A');
  ASSERT_LIEQ(a, 0, 0x41);

  lexint_char(&a, 'B');
  ASSERT_LIEQ(a, 0, 0x4142);

  lexint_char(&a, 'C');
  ASSERT_LIEQ(a, 0, 0x414243);

  lexint_char(&a, 'D');
  ASSERT_LIEQ(a, 0, 0x41424344);

  lexint_char(&a, 'E');
  ASSERT_LIEQ(a, 0, 0x4142434445);

  lexint_char(&a, 'F');
  ASSERT_LIEQ(a, 0, 0x414243444546);

  lexint_char(&a, 'G');
  ASSERT_LIEQ(a, 0, 0x41424344454647);

  lexint_char(&a, 'H');
  ASSERT_LIEQ(a, 0, 0x4142434445464748);

  lexint_char(&a, 'I');
  ASSERT_LIEQ(a, 0x41, 0x4243444546474849);

  lexint_char(&a, 'J');
  ASSERT_LIEQ(a, 0x4142, 0x434445464748494A);

  lexint_char(&a, 'K');
  ASSERT_LIEQ(a, 0x414243, 0x4445464748494A4B);

  lexint_char(&a, 'L');
  ASSERT_LIEQ(a, 0x41424344, 0x45464748494A4B4C);

  lexint_char(&a, 'M');
  ASSERT_LIEQ(a, 0x4142434445, 0x464748494A4B4C4D);

  lexint_char(&a, 'N');
  ASSERT_LIEQ(a, 0x414243444546, 0x4748494A4B4C4D4E);

  lexint_char(&a, 'O');
  ASSERT_LIEQ(a, 0x41424344454647, 0x48494A4B4C4D4E4F);

  lexint_char(&a, 'P');
  ASSERT_LIEQ(a, 0x4142434445464748, 0x494A4B4C4D4E4F50);

  lexint_char(&a, 'Q');
  ASSERT_LIEQ(a, 0x4243444546474849, 0x4A4B4C4D4E4F5051);

  lexint_char(&a, 'R');
  ASSERT_LIEQ(a, 0x434445464748494A, 0x4B4C4D4E4F505152);
}


TEST_F(LexIntTest, LexIntAccumBase10)
{
  lexint_t a;
  lexint(&a, 0, 0);

  ASSERT_TRUE(lexint_accum(&a, 1, 10));
  ASSERT_LIEQ(a, 0, 1);

  ASSERT_TRUE(lexint_accum(&a, 2, 10));
  ASSERT_LIEQ(a, 0, 12);

  ASSERT_TRUE(lexint_accum(&a, 3, 10));
  ASSERT_LIEQ(a, 0, 123);

  lexint(&a, 0, 0xFFFFFFFFFFFFFFFF);
  ASSERT_TRUE(lexint_accum(&a, 4, 10));
  ASSERT_LIEQ(a, 9, 0xFFFFFFFFFFFFFFFA);

  lexint(&a, 0x1999999999999999, 0x9999999999999999);
  ASSERT_TRUE(lexint_accum(&a, 4, 10));
  ASSERT_LIEQ(a, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFE);

  lexint(&a, 0x1999999999999999, 0x9999999999999999);
  ASSERT_FALSE(lexint_accum(&a, 6, 10));
}


TEST_F(LexIntTest, LexIntAccumBase16)
{
  lexint_t a;
  lexint(&a, 0, 0);

  ASSERT_TRUE(lexint_accum(&a, 1, 16));
  ASSERT_LIEQ(a, 0, 1);

  ASSERT_TRUE(lexint_accum(&a, 2, 16));
  ASSERT_LIEQ(a, 0, 0x12);

  ASSERT_TRUE(lexint_accum(&a, 3, 16));
  ASSERT_LIEQ(a, 0, 0x123);

  lexint(&a, 0, 0xFFFFFFFFFFFFFFFF);
  ASSERT_TRUE(lexint_accum(&a, 4, 16));
  ASSERT_LIEQ(a, 0xF, 0xFFFFFFFFFFFFFFF4);

  lexint(&a, 0x0FFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF);
  ASSERT_TRUE(lexint_accum(&a, 4, 16));
  ASSERT_LIEQ(a, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFF4);

  lexint(&a, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF);
  ASSERT_FALSE(lexint_accum(&a, 0, 16));
}


TEST_F(LexIntTest, LexIntAccumBase2)
{
  lexint_t a;
  lexint(&a, 0, 0);

  ASSERT_TRUE(lexint_accum(&a, 1, 2));
  ASSERT_LIEQ(a, 0, 1);

  ASSERT_TRUE(lexint_accum(&a, 0, 2));
  ASSERT_LIEQ(a, 0, 2);

  ASSERT_TRUE(lexint_accum(&a, 1, 2));
  ASSERT_LIEQ(a, 0, 5);

  lexint(&a, 0, 0xFFFFFFFFFFFFFFFF);
  ASSERT_TRUE(lexint_accum(&a, 0, 2));
  ASSERT_LIEQ(a, 1, 0xFFFFFFFFFFFFFFFE);

  lexint(&a, 0x7FFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF);
  ASSERT_TRUE(lexint_accum(&a, 1, 2));
  ASSERT_LIEQ(a, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF);

  lexint(&a, 0x8000000000000000, 0);
  ASSERT_FALSE(lexint_accum(&a, 0, 2));
}


TEST_F(LexIntTest, LexIntCastDouble)
{
  lexint_t a;

  ASSERT_EQ(0.0, lexint_double(lexint(&a, 0, 0)));
  ASSERT_EQ(1.0, lexint_double(lexint(&a, 0, 1)));
  ASSERT_EQ(2.0, lexint_double(lexint(&a, 0, 2)));
  ASSERT_EQ(10.0, lexint_double(lexint(&a, 0, 10)));
  ASSERT_EQ(4503599627370495.0,
    lexint_double(lexint(&a, 0, 0xFFFFFFFFFFFFF)));
  ASSERT_EQ(18446744073709552000.0,
    lexint_double(lexint(&a, 0, 0xFFFFFFFFFFFFFFFF)));
  ASSERT_EQ(18446744073709552000.0, lexint_double(lexint(&a, 1, 0)));
  ASSERT_EQ(18446744073709552000.0, lexint_double(lexint(&a, 1, 1)));
  ASSERT_EQ(1e20, lexint_double(lexint(&a, 5, 0x6BC75E2D63100000)));
  ASSERT_EQ(3.4028236692093846e38,
    lexint_double(lexint(&a, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF)));
}
