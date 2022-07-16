struct _ActorStats
  """
  Pony struct for the Pony actor stats C struct that can be used to
  inspect statistics about an actor.
  """

  var heap_mem_allocated: USize = -1
    """
    Memory currently allocated by objects on the actor heap
    """

  var heap_mem_used: USize = -1
    """
    Memory currently used by objects on the actor heap
    """

  var heap_num_allocated: USize = -1
    """
    Number of objects currently on the actor heap
    """

  var heap_realloc_counter: USize = -1
    """
    Number of heap re-allocations made on the actor heap during the
    lifetime of the actor
    """

  var heap_alloc_counter: USize = -1
    """
    Number of heap allocations made on the actor heap during the
    lifetime of the actor
    """

  var heap_free_counter: USize = -1
    """
    Number of heap allocations freed on the actor heap during the
    lifetime of the actor
    """

  var heap_gc_counter: USize = -1
    """
    Number of times the heap has been garbage collected
    """

  var system_cpu: USize = -1
    """
    Amount of cpu used to process system messages
    """

  var app_cpu: USize = -1
    """
    Amount of cpu used to process app messages
    """

  var gc_mark_cpu: USize = -1
    """
    Amount of cpu used for mark phase to garbage collect actor heap
    """

  var gc_sweep_cpu: USize = -1
    """
    Amount of cpu used for sweep phase to garbage collect actor heap
    """

  var messages_sent_counter: USize = -1
    """
    Number of messages the actor has sent
    """

  var system_messages_processed_counter: USize = -1
    """
    Number of system messages the actor has processed
    """

  var app_messages_processed_counter: USize = -1
    """
    Number of app messages the actor has processed
    """

  var foreign_actormap_objectmap_mem_used: USize = -1
    """
    Memory in use by the objectmaps stored in the foreign actormap
    """

  var foreign_actormap_objectmap_mem_allocated: USize = -1
    """
    Memory allocated by the objectmaps stored in the foreign actormap
    """
