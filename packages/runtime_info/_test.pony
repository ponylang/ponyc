use "pony_test"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestActorStatsLinks)
    test(_TestSchedulerLinks)
    test(_TestSchedulerStatsLinks)

class \nodoc\ iso _TestSchedulerLinks is UnitTest
  """
  The only point of this test is to verify that
  each platform can link to the exported symbols.
  """
  fun name(): String => "runtime_info/SchedulerLinks"

  fun apply(h: TestHelper) =>
    Scheduler.schedulers(SchedulerInfoAuth(h.env.root))

class \nodoc\ iso _TestSchedulerStatsLinks is UnitTest
  """
  The only point of this test is to verify that
  each platform can link to the exported symbols.
  """
  fun name(): String => "runtime_info/SchedulerStatsLinks"

  fun apply(h: TestHelper) =>
    SchedulerStats.total_mem_allocated(SchedulerStatsAuth(h.env.root))

class \nodoc\ iso _TestActorStatsLinks is UnitTest
  """
  The only point of this test is to verify that
  each platform can link to the exported symbols.
  """
  fun name(): String => "runtime_info/ActorStatsLinks"

  fun apply(h: TestHelper) =>
    ActorStats.heap_mem_allocated(ActorStatsAuth(h.env.root))
