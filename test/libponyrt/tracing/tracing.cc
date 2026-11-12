#include <platform.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <tuple>

#include <asio/asio.h>
#include <tracing/tracing.h>

#ifdef GTEST_HAS_DEATH_TEST

namespace
{
  void set_gate(bool enabled)
  {
#if defined(_MSC_VER) && !defined(__clang__)
    *(volatile bool*)&ponyint_tracing_enabled = enabled;
#else
    __atomic_store_n(&ponyint_tracing_enabled, enabled, __ATOMIC_RELAXED);
#endif
  }

  struct TracingConfig
  {
    char format[5] = "json";
    char output[15] = "unused.json";
    char categories[6] = "";
    char mode[16] = "file";

    TracingConfig(bool has_categories, bool flight_recorder)
    {
      if(has_categories)
        std::strcpy(categories, "actor");
      if(flight_recorder)
        std::strcpy(mode, "flight_recorder");
      ponyint_tracing_init(format, output, categories, mode, 16, false);
    }
  };

  class TracingGateTest : public testing::TestWithParam<std::tuple<bool, bool>>
  {
  protected:
    void SetUp() override
    {
      testing::FLAGS_gtest_death_test_style = "threadsafe";
    }
  };

  class TracingInitTest : public TracingGateTest {};
  class TracingThreadStartTest : public TracingGateTest {};
  class TracingUpdateTest : public TracingGateTest {};
  class TracingMasterTest : public testing::TestWithParam<bool> {};
}

TEST_P(TracingInitTest, SetsGlobalGateBeforeThreadStartup)
{
  const bool has_categories = std::get<0>(GetParam());
  const bool flight_recorder = std::get<1>(GetParam());
  const bool expected = has_categories || flight_recorder;
  ASSERT_EXIT({
    set_gate(!expected);
    TracingConfig config(has_categories, flight_recorder);
    std::_Exit(PONYINT_TRACING_ENABLED == expected ? 0 : 1);
  }, testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(Configurations, TracingInitTest,
  testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(TracingThreadStartTest, PreservesGlobalGate)
{
  const bool has_categories = std::get<0>(GetParam());
  const bool enabled = std::get<1>(GetParam());
  ASSERT_EXIT({
    TracingConfig config(has_categories, false);
    ponyint_tracing_schedulers_init(1, UINT32_MAX);
    set_gate(enabled);
    scheduler_t asio{};
    asio.index = PONY_ASIO_SCHEDULER_INDEX;
    ponyint_tracing_thread_start(&asio);
    std::_Exit(PONYINT_TRACING_ENABLED == enabled ? 0 : 1);
  }, testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(OppositeInitialState, TracingThreadStartTest,
  testing::Values(std::make_tuple(false, true), std::make_tuple(true, false)));

TEST_P(TracingUpdateTest, PreservesGlobalGate)
{
  const bool enabled = std::get<0>(GetParam());
  const bool nonempty_update = std::get<1>(GetParam());
  ASSERT_EXIT({
    TracingConfig config(false, false);
    ponyint_tracing_schedulers_init(1, UINT32_MAX);
    scheduler_t scheduler{};
    scheduler.index = 0;
    ponyint_tracing_thread_start(&scheduler);
    set_gate(enabled);
    ponyint_tracing_apply_update(nonempty_update ? UINT64_MAX : 0);
    std::_Exit(PONYINT_TRACING_ENABLED == enabled ? 0 : 1);
  }, testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(GateAndUpdateStates, TracingUpdateTest,
  testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(TracingMasterTest, PublishesGlobalGate)
{
  testing::FLAGS_gtest_death_test_style = "threadsafe";
  const bool flight_recorder = GetParam();
  ASSERT_EXIT(([flight_recorder] {
    char program[] = "tracing-test";
    char maxthreads[] = "--ponymaxthreads";
    char one[] = "1";
    char tracing_mode[] = "--ponytracingmode";
    char file_mode[] = "file";
    char recorder_mode[] = "flight_recorder";
    char* argv[] = {program, maxthreads, one, tracing_mode,
      flight_recorder ? recorder_mode : file_mode, nullptr};
    pony_init(5, argv);
    pony_ctx()->scheduler->index = 0;

    auto check_gate = [](bool expected, int failure) {
      if(PONYINT_TRACING_ENABLED != expected)
        std::_Exit(failure);
    };

    ponyint_tracing_master_apply(SCHED_TRACING_ENABLE, UINT64_MAX);
    check_gate(true, 1);
    ponyint_tracing_master_apply(SCHED_TRACING_DISABLE, UINT64_MAX);
    check_gate(flight_recorder, 2);
    ponyint_tracing_master_apply(SCHED_TRACING_SET, UINT64_MAX);
    check_gate(true, 3);
    ponyint_tracing_master_apply(SCHED_TRACING_SET, 0);
    check_gate(flight_recorder, 4);
    std::_Exit(0);
  }()), testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(RecorderModes, TracingMasterTest, testing::Bool());

#endif
