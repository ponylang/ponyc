use @pony_actor_stats[_ActorStats]()

primitive ActorStats
  """
  Provides functions that expose statistics about the current actor. All
  methods will return '0' if the runtime used the program wasn't compiled with
  runtime statistics turned on.
  """

  fun heap_mem_allocated(auth: ActorStatsAuth): USize =>
    """
    Returns the memory currently allocated by objects on the current actor's heap
    """
    ifdef runtimestats then @pony_actor_stats().heap_mem_allocated
    else 0
    end

  fun heap_mem_used(auth: ActorStatsAuth): USize =>
    """
    Returns the memory currently used by objects on the current actor's heap
    """
    ifdef runtimestats then @pony_actor_stats().heap_mem_used
    else 0
    end

  fun heap_num_allocated(auth: ActorStatsAuth): USize =>
    """
    Returns the number of objects currently on the current actor's heap
    """
    ifdef runtimestats then @pony_actor_stats().heap_num_allocated
    else 0
    end

  fun heap_realloc_counter(auth: ActorStatsAuth): USize =>
    """
    Returns the number of heap re-allocations made on the current actor's heap during
    the lifetime of the actor
    """
    ifdef runtimestats then @pony_actor_stats().heap_realloc_counter
    else 0
    end

  fun heap_alloc_counter(auth: ActorStatsAuth): USize =>
    """
    Returns the number of heap allocations made on the current actor's heap during
    the lifetime of the actor
    """
    ifdef runtimestats then @pony_actor_stats().heap_alloc_counter
    else 0
    end

  fun heap_free_counter(auth: ActorStatsAuth): USize =>
    """
    Returns the number of heap allocations freed on the current actor's heap during
    the lifetime of the actor
    """
    ifdef runtimestats then @pony_actor_stats().heap_free_counter
    else 0
    end

  fun heap_gc_counter(auth: ActorStatsAuth): USize =>
    """
    Returns the number of times the current actor's heap has been garbage collected
    """
    ifdef runtimestats then @pony_actor_stats().heap_gc_counter
    else 0
    end

  fun system_cpu(auth: ActorStatsAuth): USize =>
    """
    Returns the amount of cpu the current actor has used to process system messages
    """
    ifdef runtimestats then @pony_actor_stats().system_cpu
    else 0
    end

  fun app_cpu(auth: ActorStatsAuth): USize =>
    """
    Returns the amount of cpu the current actor has used to process app messages
    """
    ifdef runtimestats then @pony_actor_stats().app_cpu
    else 0
    end

  fun gc_mark_cpu(auth: ActorStatsAuth): USize =>
    """
    Returns the amount of cpu the current actor has used for mark phase to garbage collect its heap
    """
    ifdef runtimestats then @pony_actor_stats().gc_mark_cpu
    else 0
    end

  fun gc_sweep_cpu(auth: ActorStatsAuth): USize =>
    """
    Returns the amount of cpu the current actor has used for sweep phase to garbage collect its heap
    """
    ifdef runtimestats then @pony_actor_stats().gc_sweep_cpu
    else 0
    end

  fun messages_sent_counter(auth: ActorStatsAuth): USize =>
    """
    Returns the number of messages the current actor has sent
    """
    ifdef runtimestats then @pony_actor_stats().messages_sent_counter
    else 0
    end

  fun system_messages_processed_counter(auth: ActorStatsAuth): USize =>
    """
    Returns the number of system messages the current actor has processed
    """
    ifdef runtimestats then @pony_actor_stats().system_messages_processed_counter
    else 0
    end

  fun app_messages_processed_counter(auth: ActorStatsAuth): USize =>
    """
    Returns the number of app messages the current actor has processed
    """
    ifdef runtimestats then @pony_actor_stats().app_messages_processed_counter
    else 0
    end
