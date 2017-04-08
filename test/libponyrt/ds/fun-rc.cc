#include "ds/fun.h"

#include "gtest/gtest.h"
#include "rapidcheck/gtest.h"
#include "rapidcheck.h"

namespace
{

RC_GTEST_PROP(RC_DsFunTest, HashSameIntGivesSameKey, (std::uint64_t i))
{
  uint64_t key1 = ponyint_hash_int64(i);
  uint64_t key2 = ponyint_hash_int64(i);

  RC_ASSERT(key1 == key2);
}

RC_GTEST_PROP(RC_DsFunTest, HashSameStringGivesSameKey, (std::string const & s))
{
  auto cs = s.c_str();

  uint64_t key1 = ponyint_hash_str(cs);
  uint64_t key2 = ponyint_hash_str(cs);

  RC_ASSERT(key1 == key2);
}

auto clp2 = [](std::size_t i) -> std::size_t
{
  // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
  i += (i == 0);
  i--;
  i |= i >> 1;
  i |= i >> 2;
  i |= i >> 4;
  i |= i >> 8;
  i |= i >> 16;
  i |= i >> 32;
  i++;

  return i;
};

RC_GTEST_PROP(RC_DsFunTest, NumbersToNextPow, ())
{
  auto const i = *rc::gen::inRange<std::size_t>(0, (1ULL << 63) + 1);

  auto const pow2 = ponyint_next_pow2(i);
  auto const expected = clp2(i);

  RC_ASSERT(expected == pow2);
}

RC_GTEST_PROP(RC_DsFunTest, PowersOfTwoAreNotRounded, ())
{
  auto const shift = *rc::gen::inRange(0, 64);
  std::size_t const i = 1ULL << shift;

  auto const pow2 = ponyint_next_pow2(i);

  auto const expected = clp2(i);

  RC_ASSERT(expected == pow2);
}

}
