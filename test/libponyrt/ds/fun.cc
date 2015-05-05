#include <platform.h>
#include <gtest/gtest.h>

#include <ds/fun.h>

/** Hashing the same integer returns the same key.
 *
 */
TEST(DsFunTest, HashSameIntGivesSameKey)
{
  uint64_t i = 2701197231051989;

  uint64_t key1 = hash_int(i);
  uint64_t key2 = hash_int(i);

  ASSERT_EQ(key1, key2);
}

/** Hasing the same string, returns the same key.
 *
 */
TEST(DsFunTest, HashSameStringGivesSameKey)
{
  const char* s = "Pony";

  uint64_t key1 = hash_str(s);
  uint64_t key2 = hash_str(s);

  ASSERT_EQ(key1, key2);
}

/** Hashing the same pointer returns the same key.
 *
 */
TEST(DsFunTest, HashSamePointerGivesSameKey)
{
  void* p = malloc(sizeof(void*));

  uint64_t key2 = hash_ptr(p);
  uint64_t key1 = hash_ptr(p);

  ASSERT_EQ(key1, key2);

  free(p);
}

/** Numbers are rounded to the next power of two.
 *
 */
TEST(DsFunTest, NumbersToNextPow)
{
  ASSERT_EQ(1, next_pow2(0));
  ASSERT_EQ(4, next_pow2(3));
  ASSERT_EQ(256, next_pow2(129));
  ASSERT_EQ(256, next_pow2(200));
  ASSERT_EQ(256, next_pow2(255));
}

/** Powers of two are not rounded.
 *
 */
TEST(DsFunTest, PowersOfTwoAreNotRounded)
{
  ASSERT_EQ(1, next_pow2(1));
  ASSERT_EQ(2, next_pow2(2));
  ASSERT_EQ(128, next_pow2(128));
}
