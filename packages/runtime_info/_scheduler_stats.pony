struct _SchedulerStats
  """
  Pony struct for the Pony scheduler stats C struct that can be used to
  inspect statistics about a scheduler thread.
  """

  var mem_used: I64 = -1
    """
    Memory used by mutemaps
    """

  var mem_allocated: I64 = -1
    """
    Memory allocated for mutemaps
    """

  var mem_used_actors: I64 = -1
    """
    Memory used for actors created and for GC acquire/release actormaps
    """

  var mem_allocated_actors: I64 = -1
    """
    Memory allocated for actors created and for GC acquire/release actormaps
    """

  var created_actors_counter: USize = -1
    """
    Count of actors created
    """

  var destroyed_actors_counter: USize = -1
    """
    Count of actors destroyed
    """

  var actor_app_cpu: USize = -1
    """
    Amount of cpu used to process actor app messages
    """

  var actor_gc_mark_cpu: USize = -1
    """
    Amount of cpu used for mark phase to garbage collect actor heaps
    """

  var actor_gc_sweep_cpu: USize = -1
    """
    Amount of cpu used sweep phase to garbage collect actor heaps
    """

  var actor_system_cpu: USize = -1
    """
    Amount of cpu used to process actor system messages
    """

  var msg_cpu: USize = -1
    """
    Amount of cpu used to process scheduler system messages
    """

  var misc_cpu: USize = -1
    """
    Amount of cpu used to by scheduler while waiting to do work
    """

  var mem_used_inflight_messages: I64 = -1
    """
    Memory used by inflight messages
    """

  var mem_allocated_inflight_messages: I64 = -1
    """
    Memory allocated for inflight messages
    """

  var num_inflight_messages: I64 = -1
    """
    Number of inflight messages
    """

