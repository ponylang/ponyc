#include <platform.h>

#include <sched/scheduler.h>

#include <gtest/gtest.h>

TEST(Scheduler, MpmcqAlignment)
{
  // An mpmcq_t is embedded in the scheduler_t structure.
  // For the performance and scheduling code we need to ensure that this is 64-bit aligned.
#ifndef PLATFORM_IS_VISUAL_STUDIO // TODO(sgebbie) Hmmm, I need to work out the correct syntax for 'alignof' under Visual Studio
  ASSERT_EQ(64, alignof(scheduler_t::q));
  scheduler_t sched;
  ASSERT_EQ(64, alignof(sched.q));
#endif
}

TEST(Scheduler, QuiescenceSize)
{
  // The scheduler parameter controling the quiescence cycle timeout is communicated via
  // pony_msgi_t and therefore we must make sure that the integer storage is sufficiently large.
  // If this test fails on any platform then we will need to create a new, more suitable, pony_msgx_t
  ASSERT_EQ(sizeof(uint64_t), sizeof(scheduler_t::quiescence_cycles));
  ASSERT_EQ(sizeof(intptr_t), sizeof(pony_msgi_t::i));
  ASSERT_TRUE(sizeof(scheduler_t::quiescence_cycles) <= sizeof(pony_msgi_t::i));
}
