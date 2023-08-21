use @pony_scheduler_stats[_SchedulerStats]()

primitive SchedulerStats
  """
  Provides functions that expose statistics about the current scheduler. All
  methods will return '0' if the runtime used the program wasn't compiled with
  runtime statistics turned on.
  """

  fun total_mem_allocated(auth: SchedulerStatsAuth): I64 =>
    """
    Returns the total memory currently allocated by the current scheduler thread
    (note: this is an approximate value and it can be negative as it represents
    only a partial view across all the scheduler threads)
    """
    ifdef runtimestats then @pony_scheduler_stats().mem_allocated + @pony_scheduler_stats().mem_allocated_actors
    else 0
    end

  fun total_mem_used(auth: SchedulerStatsAuth): I64 =>
    """
    Returns the total memory currently used by the current scheduler thread
    (note: this is an approximate value and it can be negative as it represents
    only a partial view across all the scheduler threads)
    """
    ifdef runtimestats then @pony_scheduler_stats().mem_used + @pony_scheduler_stats().mem_used_actors
    else 0
    end

  fun created_actors_counter(auth: SchedulerStatsAuth): USize =>
    """
    Returns the number of actors created by the current schedule thread
    (note: this is an approximate value and it can be negative as it represents
    only a partial view across all the scheduler threads)
    """
    ifdef runtimestats then @pony_scheduler_stats().created_actors_counter
    else 0
    end

  fun destroyed_actors_counter(auth: SchedulerStatsAuth): USize =>
    """
    Returns the number of actors destroyed by the current schedule thread
    """
    ifdef runtimestats then @pony_scheduler_stats().destroyed_actors_counter
    else 0
    end

  fun actor_app_cpu(auth: SchedulerStatsAuth): USize =>
    """
    Returns the amount of cpu the current scheduler has used to process actor app messages
    """
    ifdef runtimestats then @pony_scheduler_stats().actor_app_cpu
    else 0
    end

  fun actor_gc_mark_cpu(auth: SchedulerStatsAuth): USize =>
    """
    Returns the amount of cpu the current scheduler has used for mark phase to garbage collect actor heaps
    """
    ifdef runtimestats then @pony_scheduler_stats().actor_gc_mark_cpu
    else 0
    end

  fun actor_gc_sweep_cpu(auth: SchedulerStatsAuth): USize =>
    """
    Returns the amount of cpu the current scheduler has used for sweep phase to garbage collect actor heaps
    """
    ifdef runtimestats then @pony_scheduler_stats().actor_gc_sweep_cpu
    else 0
    end

  fun actor_system_cpu(auth: SchedulerStatsAuth): USize =>
    """
    Returns the amount of cpu the current scheduler has used to process actor system messages
    """
    ifdef runtimestats then @pony_scheduler_stats().actor_system_cpu
    else 0
    end

  fun msg_cpu(auth: SchedulerStatsAuth): USize =>
    """
    Returns the amount of cpu the current scheduler has used to process scheduler system messages
    """
    ifdef runtimestats then @pony_scheduler_stats().msg_cpu
    else 0
    end

  fun misc_cpu(auth: SchedulerStatsAuth): USize =>
    """
    Returns the amount of cpu the current scheduler has used while waiting to do work
    """
    ifdef runtimestats then @pony_scheduler_stats().misc_cpu
    else 0
    end

  fun mem_used_inflight_messages(auth: SchedulerStatsAuth): I64 =>
    """
    Returns the number of memory used by inflight messages created by the current
    scheduler thread (note: this is an approximate value and it can be negative if
    the current thread receives more messages than it sends)
    """
    ifdef runtimestatsmessages then @pony_scheduler_stats().mem_used_inflight_messages
    else 0
    end

  fun mem_allocated_inflight_messages(auth: SchedulerStatsAuth): I64 =>
    """
    Returns the number of memory allocated by inflight messages created by the current
    scheduler thread (note: this is an approximate value and it can be negative if
    the current thread receives more messages than it sends)
    """
    ifdef runtimestatsmessages then @pony_scheduler_stats().mem_allocated_inflight_messages
    else 0
    end

  fun num_inflight_messages(auth: SchedulerStatsAuth): I64 =>
    """
    Returns the number of inflight messages by the current scheduler thread
    (note: this is an approximate value and it can be negative if
    the current thread receives more messages than it sends)
    """
    ifdef runtimestatsmessages then @pony_scheduler_stats().num_inflight_messages
    else 0
    end
