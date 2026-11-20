#include <actor/actor.h>
#include <gc/root_audit.h>
#include <sched/scheduler.h>

#include <gtest/gtest.h>

TEST(RootAudit, RefusesOtherActor)
{
  pony_ctx_t ctx{};
  pony_actor_t current{};
  pony_actor_t other{};
  ctx.current = &current;
  void* roots[] = {&other};
  const char* modes[] = {"receiver"};

  testing::internal::CaptureStderr();
  ponyint_gc_root_audit(&ctx, &other, "example", 1, nullptr, 1, roots, modes);
  EXPECT_EQ("gc-root-audit example 1 refused receiver-mismatch receiver=0\n",
    testing::internal::GetCapturedStderr());
}
