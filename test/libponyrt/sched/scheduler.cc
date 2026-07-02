#include <platform.h>
#include <gtest/gtest.h>

#include <sched/scheduler.h>

/** A one-second interval converts to ~2e9 cycles, the scale the scheduler's
 * other timing thresholds use.
 */
TEST(SchedStatsIntervalTest, OneSecondIsTwoBillionCycles)
{
  ASSERT_EQ(UINT64_C(2000000000),
    ponyint_sched_stats_interval_cycles(1));
}

/** The conversion is done at 64-bit width: an interval whose cycle count
 * exceeds 2^32 must not be truncated. 3 seconds is 6e9 cycles, which is past
 * 2^32; a 32-bit multiply would truncate it to ~1.7e9.
 */
TEST(SchedStatsIntervalTest, LargeIntervalDoesNotTruncate)
{
  ASSERT_EQ(UINT64_C(6000000000),
    ponyint_sched_stats_interval_cycles(3));
}

/** The widest accepted interval (UINT32_MAX is the sentinel that disables
 * stats, so UINT32_MAX - 1 is the largest real value) still fits in 64 bits.
 */
TEST(SchedStatsIntervalTest, MaxIntervalDoesNotOverflow)
{
  ASSERT_EQ(UINT64_C(8589934588000000000),
    ponyint_sched_stats_interval_cycles(UINT32_MAX - 1));
}
