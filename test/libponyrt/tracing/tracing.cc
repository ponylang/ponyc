#include <platform.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <tuple>

#include <asio/asio.h>
#include <asio/event.h>
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
  class TracingAsioTest : public testing::TestWithParam<bool> {};

  void require_asio(bool condition, const char* message)
  {
    if(!condition)
    {
      std::fprintf(stderr, "%s\n", message);
      std::_Exit(1);
    }
  }

  std::string asio_trace_path(const char* prefix)
  {
    auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    return testing::TempDir() + prefix + std::to_string(nonce) + ".json";
  }

#ifndef USE_SYSTEMATIC_TESTING
  char* asio_behavior_name(uint32_t id)
  {
    (void)id;
    static char name[] = "asio_probe";
    return name;
  }

  void asio_probe(pony_actor_t* owner, uint32_t id)
  {
    asio_event_t* event = pony_asio_event_create(owner, -1, ASIO_TIMER,
      UINT64_C(86400000000000), false);
    require_asio(event != nullptr, "could not create ASIO probe timer");
    event->msg_id = id;
    pony_asio_event_unsubscribe(event);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while(std::chrono::steady_clock::now() < deadline)
    {
      pony_msg_t* msg = ponyint_actor_messageq_pop(&owner->q);
      if(msg == nullptr)
        continue;
      require_asio(msg->id == id, "unexpected ASIO probe message id");
      asio_msg_t* notification = reinterpret_cast<asio_msg_t*>(msg);
      require_asio(notification->event == event,
        "unexpected ASIO probe event");
      require_asio(notification->flags == ASIO_DISPOSABLE,
        "ASIO probe did not receive timer disposal");
      pony_asio_event_destroy(event);
      return;
    }
    require_asio(false, "timed out waiting for ASIO probe disposal");
  }
#endif
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

TEST_P(TracingAsioTest, AppliesRuntimeCategoryChangesThroughQueue)
{
#ifdef USE_SYSTEMATIC_TESTING
  GTEST_SKIP() << "Systematic testing does not run the ASIO thread";
#else
  testing::FLAGS_gtest_death_test_style = "threadsafe";
  const bool initially_enabled = GetParam();
  ASSERT_EXIT(([initially_enabled] {
    std::string output = asio_trace_path("ponyc-asio-tracing-");
    char program[] = "tracing-asio-test";
    char maxthreads[] = "--ponymaxthreads";
    char one[] = "1";
    char output_option[] = "--ponytracingoutput";
    char categories_option[] = "--ponytracingcategories";
    char categories[] = "scheduler,actor_behavior";
    char empty[] = "";
    char* argv[] = {program, maxthreads, one, output_option, &output[0],
      categories_option, initially_enabled ? categories : empty, nullptr};
    pony_init(7, argv);
    pony_ctx()->scheduler->index = 0;
    require_asio(ponyint_tracing_start(), "could not start tracing");
    require_asio(ponyint_asio_start(), "could not start ASIO");

    char name[] = "AsioTracingProbe";
    _pony_type_t type{};
    type.size = sizeof(pony_actor_t);
    type.name = name;
    type.get_behavior_name = asio_behavior_name;
    type.event_notify = 100;
    pony_actor_t* owner = pony_create(pony_ctx(), &type, false);
    ponyint_become(pony_ctx(), owner);

    constexpr uint64_t scheduler = UINT64_C(1) << 0;
    constexpr uint64_t behavior = UINT64_C(1) << 3;
    asio_probe(owner, 100);
    require_asio(pony_tracing_enable_categories(scheduler | behavior) ==
      PONY_TRACING_OK,
      "could not enable ASIO tracing categories");
    asio_probe(owner, 101);
    ponyint_tracing_master_apply(SCHED_TRACING_DISABLE, behavior);
    asio_probe(owner, 102);
    ponyint_tracing_master_apply(SCHED_TRACING_ENABLE, behavior);
    asio_probe(owner, 103);
    ponyint_tracing_master_apply(SCHED_TRACING_SET, scheduler);
    asio_probe(owner, 104);
    ponyint_tracing_master_apply(SCHED_TRACING_SET, scheduler | behavior);
    asio_probe(owner, 105);
    ponyint_tracing_master_apply(SCHED_TRACING_SET, 0);
    asio_probe(owner, 106);
    ponyint_tracing_master_apply(SCHED_TRACING_ENABLE, scheduler);
    asio_probe(owner, 107);
    ponyint_tracing_master_apply(SCHED_TRACING_ENABLE, behavior);
    asio_probe(owner, 108);

    require_asio(ponyint_asio_stop(), "could not stop ASIO");
    ponyint_tracing_master_apply(SCHED_TRACING_SET, scheduler | behavior);
    ponyint_tracing_stop();
    ponyint_become(pony_ctx(), nullptr);
    require_asio(ponyint_messageq_markempty(&owner->q),
      "ASIO probe queue was not empty after shutdown");
    ponyint_actor_setpendingdestroy(owner);
    ponyint_actor_destroy(owner, ACTOR_DESTROYED_MANUAL);

    std::ifstream trace(output);
    require_asio(trace.good(), "ASIO trace output is missing");
    unsigned counts[9]{};
    std::string event_tids[9];
    std::string asio_tid;
    std::string asio_index = "\"index\":" +
      std::to_string(static_cast<int32_t>(PONY_ASIO_SCHEDULER_INDEX)) + "}";
    std::string line;
    while(std::getline(trace, line))
    {
      size_t tid_start = line.find("\"tid\":");
      std::string tid;
      if(tid_start != std::string::npos)
      {
        tid_start += 6;
        tid = line.substr(tid_start, line.find(',', tid_start) - tid_start);
      }
      if(line.find("\"name\":\"thread_stop\"") != std::string::npos &&
        line.find(asio_index) != std::string::npos)
        asio_tid = tid;
      if(line.find("\"name\":\"actor_behavior_schedule\"") ==
        std::string::npos)
        continue;
      if(line.find("\"from_actor_type_name\":\"@@ASIO INJECT@@\"") ==
        std::string::npos)
        continue;
      for(unsigned phase = 0; phase < 9; phase++)
      {
        std::string id = "\"behavior_id\":" + std::to_string(100 + phase)
          + ",";
        if(line.find(id) != std::string::npos)
        {
          counts[phase]++;
          event_tids[phase] = tid;
        }
      }
    }
    trace.close();
    std::remove(output.c_str());
    require_asio(!asio_tid.empty(), "ASIO thread stop trace is missing");
    const unsigned expected[] = {initially_enabled ? 1u : 0u,
      1, 0, 1, 0, 1, 0, 0, 1};
    for(unsigned phase = 0; phase < 9; phase++)
    {
      if(counts[phase] != expected[phase])
      {
        std::fprintf(stderr, "ASIO phase %u: expected %u events, found %u\n",
          phase, expected[phase], counts[phase]);
        std::_Exit(1);
      }
      if(expected[phase] != 0)
        require_asio(event_tids[phase] == asio_tid,
          "probe trace came from a thread other than ASIO");
    }
    std::_Exit(0);
  }()), testing::ExitedWithCode(0), "");
#endif
}

INSTANTIATE_TEST_SUITE_P(InitialCategoryStates, TracingAsioTest,
  testing::Bool());

TEST(TracingAsioFinaliseTest, DrainsControlMessagesWhileGlobalGateIsOff)
{
  testing::FLAGS_gtest_death_test_style = "threadsafe";
  ASSERT_EXIT(([] {
    std::string output = asio_trace_path("ponyc-asio-finalise-");
    char program[] = "tracing-asio-finalise-test";
    char maxthreads[] = "--ponymaxthreads";
    char one[] = "1";
    char output_option[] = "--ponytracingoutput";
    char categories_option[] = "--ponytracingcategories";
    char categories[] = "actor";
    char* argv[] = {program, maxthreads, one, output_option, &output[0],
      categories_option, categories, nullptr};
    pony_init(7, argv);
    require_asio(ponyint_tracing_start(), "could not start finalise tracing");

    messageq_t queue;
    ponyint_messageq_init(&queue);
    pony_msg_u64_t* update = reinterpret_cast<pony_msg_u64_t*>(pony_alloc_msg(
      POOL_INDEX(sizeof(pony_msg_u64_t)), ASIO_MSG_TRACING_UPDATE));
    update->value = 0;
    ponyint_thread_messageq_push(&queue, &update->msg, &update->msg);
    asio_msg_t* io = reinterpret_cast<asio_msg_t*>(pony_alloc_msg(
      POOL_INDEX(sizeof(asio_msg_t)), ASIO_MSG_EVENT));
    io->event = nullptr;
    io->flags = ASIO_DISPOSABLE;
    ponyint_thread_messageq_push(&queue, &io->msg, &io->msg);
    update = reinterpret_cast<pony_msg_u64_t*>(pony_alloc_msg(
      POOL_INDEX(sizeof(pony_msg_u64_t)), ASIO_MSG_TRACING_UPDATE));
    update->value = UINT64_C(1) << 0;
    ponyint_thread_messageq_push(&queue, &update->msg, &update->msg);
    set_gate(false);

    ponyint_asio_finalise_tracing(&queue);
    require_asio(!PONYINT_TRACING_ENABLED,
      "finalise changed the global tracing gate");
    require_asio(ponyint_messageq_isempty(&queue),
      "finalise left messages in the control queue");
    ponyint_messageq_destroy(&queue, false);
    ponyint_tracing_thread_stop();
    ponyint_tracing_stop();

    std::ifstream trace(output);
    require_asio(trace.good(), "finalise trace output is missing");
    std::string content((std::istreambuf_iterator<char>(trace)),
      std::istreambuf_iterator<char>());
    trace.close();
    std::remove(output.c_str());
    require_asio(content.find("\"name\":\"thread_stop\"") !=
      std::string::npos,
      "finalise did not apply the queued scheduler category");
    std::_Exit(0);
  }()), testing::ExitedWithCode(0), "");
}

#endif
