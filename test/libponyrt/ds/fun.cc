#include <platform.h>
#include <gtest/gtest.h>

#include <ds/fun.h>

/** Hashing the same integer returns the same key.
 *
 */
TEST(DsFunTest, HashSameIntGivesSameKey)
{
  uint64_t i = 2701197231051989;

  uint64_t key1 = ponyint_hash_int64(i);
  uint64_t key2 = ponyint_hash_int64(i);

  ASSERT_EQ(key1, key2);
}

/** Hasing the same string, returns the same key.
 *
 */
TEST(DsFunTest, HashSameStringGivesSameKey)
{
  const char* s = "Pony";

  uint64_t key1 = ponyint_hash_str(s);
  uint64_t key2 = ponyint_hash_str(s);

  ASSERT_EQ(key1, key2);
}

/** Hashing the same pointer returns the same key.
 *
 */
TEST(DsFunTest, HashSamePointerGivesSameKey)
{
  void* p = malloc(sizeof(void*));

  uint64_t key2 = ponyint_hash_ptr(p);
  uint64_t key1 = ponyint_hash_ptr(p);

  ASSERT_EQ(key1, key2);

  free(p);
}

/** Numbers are rounded to the next power of two.
 *
 */
TEST(DsFunTest, NumbersToNextPow)
{
  ASSERT_EQ(4, (unsigned int)ponyint_next_pow2(3));
  ASSERT_EQ(8, (unsigned int)ponyint_next_pow2(5));
  ASSERT_EQ(16, (unsigned int)ponyint_next_pow2(10));
  ASSERT_EQ(32, (unsigned int)ponyint_next_pow2(25));
  ASSERT_EQ(64, (unsigned int)ponyint_next_pow2(50));
  ASSERT_EQ(128, (unsigned int)ponyint_next_pow2(100));
  ASSERT_EQ(256, (unsigned int)ponyint_next_pow2(200));
}

/** Powers of two are not rounded.
 *
 */
TEST(DsFunTest, PowersOfTwoAreNotRounded)
{
  ASSERT_EQ((size_t)128, ponyint_next_pow2(128));
}
