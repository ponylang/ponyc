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
  uint64_t i = 200;

  ASSERT_EQ(
    1,
    __pony_popcount64(
      (unsigned int)next_pow2(i)
    )
  );
}

/** Powers of two are not rounded.
 *
 */
TEST(DsFunTest, PowersOfTwoAreNotRounded)
{
  ASSERT_EQ((size_t)128, next_pow2(128));
}
