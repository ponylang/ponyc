use "runtime_info"

actor Main
  new create(env: Env) =>
    Stat(env)
    Stat(env)
    Stat(env)
    Stat(env)
    Stat(env)

actor Stat
  new create(env: Env) =>
    print_actor_stats(env)
    print_scheduler_stats(env)

    print_actor_stats(env)
    print_scheduler_stats(env)

    print_actor_stats(env)
    print_scheduler_stats(env)

    print_actor_stats(env)
    print_scheduler_stats(env)

    print_actor_stats(env)
    print_scheduler_stats(env)

  be print_actor_stats(env: Env) =>
    var ha = ActorStats.heap_mem_allocated(ActorStatsAuth(env.root))
    var hu = ActorStats.heap_mem_used(ActorStatsAuth(env.root))
    var hn = ActorStats.heap_num_allocated(ActorStatsAuth(env.root))
    var rc = ActorStats.heap_realloc_counter(ActorStatsAuth(env.root))
    var ac = ActorStats.heap_alloc_counter(ActorStatsAuth(env.root))
    var fc = ActorStats.heap_free_counter(ActorStatsAuth(env.root))
    var gc = ActorStats.heap_gc_counter(ActorStatsAuth(env.root))
    var asc = ActorStats.system_cpu(ActorStatsAuth(env.root))
    var aac = ActorStats.app_cpu(ActorStatsAuth(env.root))
    var agmc = ActorStats.gc_mark_cpu(ActorStatsAuth(env.root))
    var agsc = ActorStats.gc_sweep_cpu(ActorStatsAuth(env.root))
    var msc = ActorStats.messages_sent_counter(ActorStatsAuth(env.root))
    var smpc = ActorStats.system_messages_processed_counter(ActorStatsAuth(env.root))
    var ampc = ActorStats.app_messages_processed_counter(ActorStatsAuth(env.root))
    env.out.print("Actor stats:"
      + "\n  id: " + (digestof this).string()
      + "\n  heap memory allocated: " + ha.string()
      + "\n  heap memory used: " + hu.string()
      + "\n  heap num allocated: " + hn.string()
      + "\n  heap realloc counter: " + rc.string()
      + "\n  heap alloc counter: " + ac.string()
      + "\n  heap free counter: " + fc.string()
      + "\n  heap gc counter: " + gc.string()
      + "\n  system cpu: " + asc.string()
      + "\n  app cpu: " + aac.string()
      + "\n  garbage collection marking cpu: " + agmc.string()
      + "\n  garbage collection sweeping cpu: " + agsc.string()
      + "\n  messages sent counter: " + msc.string()
      + "\n  system messages processed counter: " + smpc.string()
      + "\n  app messages processed counter: " + ampc.string())

  be print_scheduler_stats(env: Env) =>
    var i = Scheduler.scheduler_index(SchedulerInfoAuth(env.root))
    var ta = SchedulerStats.total_mem_allocated(SchedulerStatsAuth(env.root))
    var tu = SchedulerStats.total_mem_used(SchedulerStatsAuth(env.root))
    var cac = SchedulerStats.created_actors_counter(SchedulerStatsAuth(env.root))
    var dac = SchedulerStats.destroyed_actors_counter(SchedulerStatsAuth(env.root))
    var aac = SchedulerStats.actor_app_cpu(SchedulerStatsAuth(env.root))
    var agmc = SchedulerStats.actor_gc_mark_cpu(SchedulerStatsAuth(env.root))
    var agsc = SchedulerStats.actor_gc_sweep_cpu(SchedulerStatsAuth(env.root))
    var asc = SchedulerStats.actor_system_cpu(SchedulerStatsAuth(env.root))
    var mc = SchedulerStats.msg_cpu(SchedulerStatsAuth(env.root))
    var msc = SchedulerStats.misc_cpu(SchedulerStatsAuth(env.root))
    var mum = SchedulerStats.mem_used_inflight_messages(SchedulerStatsAuth(env.root))
    var mam = SchedulerStats.mem_allocated_inflight_messages(SchedulerStatsAuth(env.root))
    var nim = SchedulerStats.num_inflight_messages(SchedulerStatsAuth(env.root))
    env.out.print("Scheduler stats:"
      + "\n  index: " + i.string()
      + "\n  total memory allocated: " + ta.string()
      + "\n  total memory used: " + tu.string()
      + "\n  created actors counter: " + cac.string()
      + "\n  destroyed actors counter: " + dac.string()
      + "\n  actors app cpu: " + aac.string()
      + "\n  actors gc marking cpu: " + agmc.string()
      + "\n  actors gc sweeping cpu: " + agsc.string()
      + "\n  actors system cpu: " + asc.string()
      + "\n  scheduler msgs cpu: " + mc.string()
      + "\n  scheduler misc cpu: " + msc.string()
      + "\n  memory used inflight messages: " + mum.string()
      + "\n  memory allocated influght messages: " + mam.string()
      + "\n  number of inflight messages: " + nim.string())
