#include <platform.h>
#include <gtest/gtest.h>

#include <sched/cpu.h>

/** Two identical readings mean no time elapsed, at every width.
 *
 */
TEST(CpuTickDiffTest, EqualReadingsAreZero)
{
  ASSERT_EQ(UINT64_C(0), ponyint_cpu_tick_diff_bits(100, 100, 32));
  ASSERT_EQ(UINT64_C(0), ponyint_cpu_tick_diff_bits(100, 100, 38));
  ASSERT_EQ(UINT64_C(0), ponyint_cpu_tick_diff_bits(100, 100, 64));
}

/** Without a wrap the diff is just the difference, at every width.
 *
 */
TEST(CpuTickDiffTest, NoWrapIsPlainDifference)
{
  ASSERT_EQ(UINT64_C(150), ponyint_cpu_tick_diff_bits(100, 250, 32));
  ASSERT_EQ(UINT64_C(4000), ponyint_cpu_tick_diff_bits(1000, 5000, 38));
  ASSERT_EQ(UINT64_C(4000), ponyint_cpu_tick_diff_bits(1000, 5000, 64));

  // An interval occupying bits above 32: widths 38 (the production ARM width)
  // and 64 preserve it, where width 32 would mask it to zero. This makes the
  // 38-bit case fail if the mask width ever regresses to 32.
  ASSERT_EQ(UINT64_C(0x200000000),
    ponyint_cpu_tick_diff_bits(0, UINT64_C(0x200000000), 38));
  ASSERT_EQ(UINT64_C(0x200000000),
    ponyint_cpu_tick_diff_bits(0, UINT64_C(0x200000000), 64));
}

/** When a narrow counter wraps, the later reading is numerically smaller than
 * the earlier one; the diff must still recover the true (small) elapsed value.
 */
TEST(CpuTickDiffTest, WrapRecoversTrueElapsed)
{
  // 32-bit: earlier 16 below 2^32, later 32 past the wrap -> 48 elapsed.
  ASSERT_EQ(UINT64_C(48),
    ponyint_cpu_tick_diff_bits(UINT64_C(0xFFFFFFF0), UINT64_C(0x20), 32));

  // 38-bit: earlier 10 below 2^38, later 5 past the wrap -> 15 elapsed.
  ASSERT_EQ(UINT64_C(15),
    ponyint_cpu_tick_diff_bits(UINT64_C(0x3FFFFFFFF6), UINT64_C(5), 38));
}

/** The tightest wrap: earlier at the counter's max, later at zero, is one tick.
 *
 */
TEST(CpuTickDiffTest, WrapBoundaryIsOneTick)
{
  ASSERT_EQ(UINT64_C(1),
    ponyint_cpu_tick_diff_bits(UINT64_C(0xFFFFFFFF), 0, 32));
  ASSERT_EQ(UINT64_C(1),
    ponyint_cpu_tick_diff_bits(UINT64_C(0x3FFFFFFFFF), 0, 38));
}

/** A 64-bit counter never wraps in practice, so the result is the unsigned
 * subtraction taken modulo 2^64 -- and the bits >= 64 path must not evaluate
 * the undefined `1 << 64`.
 */
TEST(CpuTickDiffTest, SixtyFourBitIsModuloSubtraction)
{
  ASSERT_EQ(UINT64_C(1000000000000),
    ponyint_cpu_tick_diff_bits(0, UINT64_C(1000000000000), 64));
  ASSERT_EQ(UINT64_C(1),
    ponyint_cpu_tick_diff_bits(UINT64_MAX, 0, 64));

  // A wrap near the top of the 64-bit range recovers the small true elapsed.
  ASSERT_EQ(UINT64_C(32), ponyint_cpu_tick_diff_bits(
    UINT64_C(0xFFFFFFFFFFFFFFF0), UINT64_C(0x10), 64));
}

/** The platform-width wrapper agrees with a plain difference for an ordinary
 * interval on any host.
 */
TEST(CpuTickDiffTest, PublicDiffMatchesPlainDifference)
{
  ASSERT_EQ(UINT64_C(0), ponyint_cpu_tick_diff(500, 500));
  ASSERT_EQ(UINT64_C(150), ponyint_cpu_tick_diff(100, 250));
}
