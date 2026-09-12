use "runtime_info"

actor Main
  new create(env: Env) =>
    Stat(env)
    Stat(env)
    Stat(env)
    Stat(env)
    Stat(env)

actor Stat
  """
  Collects and prints actor and scheduler statistics.
  """

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
    """
    Prints heap and CPU statistics for this actor.
    """
    let auth = ActorStatsAuth(env.root)
    var ha = ActorStats.heap_mem_allocated(auth)
    var hu = ActorStats.heap_mem_used(auth)
    var hn = ActorStats.heap_num_allocated(auth)
    var rc = ActorStats.heap_realloc_counter(auth)
    var ac = ActorStats.heap_alloc_counter(auth)
    var fc = ActorStats.heap_free_counter(auth)
    var gc = ActorStats.heap_gc_counter(auth)
    var asc = ActorStats.system_cpu(auth)
    var aac = ActorStats.app_cpu(auth)
    var agmc = ActorStats.gc_mark_cpu(auth)
    var agsc = ActorStats.gc_sweep_cpu(auth)
    var msc = ActorStats.messages_sent_counter(auth)
    var smpc =
      ActorStats.system_messages_processed_counter(auth)
    var ampc =
      ActorStats.app_messages_processed_counter(auth)
    env.out.print("Actor stats:" +
      "\n  id: " + (digestof this).string() +
      "\n  heap memory allocated: " + ha.string() +
      "\n  heap memory used: " + hu.string() +
      "\n  heap num allocated: " + hn.string() +
      "\n  heap realloc counter: " + rc.string() +
      "\n  heap alloc counter: " + ac.string() +
      "\n  heap free counter: " + fc.string() +
      "\n  heap gc counter: " + gc.string() +
      "\n  system cpu: " + asc.string() +
      "\n  app cpu: " + aac.string() +
      "\n  garbage collection marking cpu: " + agmc.string() +
      "\n  garbage collection sweeping cpu: " + agsc.string() +
      "\n  messages sent counter: " + msc.string() +
      "\n  system messages processed counter: " + smpc.string() +
      "\n  app messages processed counter: " + ampc.string())

  be print_scheduler_stats(env: Env) =>
    """
    Prints memory, actor, and CPU statistics for this scheduler.
    """
    let si_auth = SchedulerInfoAuth(env.root)
    let ss_auth = SchedulerStatsAuth(env.root)
    var i = Scheduler.scheduler_index(si_auth)
    var ta = SchedulerStats.total_mem_allocated(ss_auth)
    var tu = SchedulerStats.total_mem_used(ss_auth)
    var cac =
      SchedulerStats.created_actors_counter(ss_auth)
    var dac =
      SchedulerStats.destroyed_actors_counter(ss_auth)
    var aac = SchedulerStats.actor_app_cpu(ss_auth)
    var agmc =
      SchedulerStats.actor_gc_mark_cpu(ss_auth)
    var agsc =
      SchedulerStats.actor_gc_sweep_cpu(ss_auth)
    var asc = SchedulerStats.actor_system_cpu(ss_auth)
    var mc = SchedulerStats.msg_cpu(ss_auth)
    var msc = SchedulerStats.misc_cpu(ss_auth)
    var mum =
      SchedulerStats.mem_used_inflight_messages(ss_auth)
    var mam =
      SchedulerStats.mem_allocated_inflight_messages(
        ss_auth)
    var nim =
      SchedulerStats.num_inflight_messages(ss_auth)
    env.out.print("Scheduler stats:" +
      "\n  index: " + i.string() +
      "\n  total memory allocated: " + ta.string() +
      "\n  total memory used: " + tu.string() +
      "\n  created actors counter: " + cac.string() +
      "\n  destroyed actors counter: " + dac.string() +
      "\n  actors app cpu: " + aac.string() +
      "\n  actors gc marking cpu: " + agmc.string() +
      "\n  actors gc sweeping cpu: " + agsc.string() +
      "\n  actors system cpu: " + asc.string() +
      "\n  scheduler msgs cpu: " + mc.string() +
      "\n  scheduler misc cpu: " + msc.string() +
      "\n  memory used inflight messages: " + mum.string() +
      "\n  memory allocated influght messages: " + mam.string() +
      "\n  number of inflight messages: " + nim.string())
