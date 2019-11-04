provider pony {
  /**
   * Fired when a actor is being created
   * @param scheduler is the scheduler that created the actor
   * @actor is the actor that was created
   */
  probe actor__alloc(uintptr_t scheduler, uintptr_t actor);

  /**
   * Fired when a message is being send
   * @param scheduler is the active scheduler
   * @param id the message id
   * @param actor_from is the sending actor
   * @param actor_to is the receiving actor
   */
  probe actor__msg__send(uintptr_t scheduler, uint32_t id, uintptr_t actor_from, uintptr_t actor_to);

  /**
   * Fired when a message is being run by an actor
   * @param actor the actor running the message
   * @param id the message id
   */
  probe actor__msg__run(uintptr_t scheduler, uintptr_t actor, uint32_t id);

  /**
   * Fired when a message is being sent to an actor
   * @param scheduler is the active scheduler's index
   * @param id the message id
   * @param actor_from is the sending actor
   * @param actor_to is the receiving actor
   */
  probe actor__msg__push(int32_t scheduler_index, uint32_t id, uintptr_t actor_from, uintptr_t actor_to);

  /**
   * Fired when a message is being run by an actor
   * @param scheduler is the active scheduler's index
   * @param id the message id
   * @param actor_to is the receiving actor
   */
  probe actor__msg__pop(int32_t scheduler_index, uint32_t id, uintptr_t actor);

  /**
   * Fired when a message is being sent to an thread
   * @param id the message id
   * @param thread_from is the sending thread index
   * @param thread_to is the receiving thread index
   */
  probe thread__msg__push(uint32_t id, uintptr_t thread_from, uintptr_t thread_to);

  /**
   * Fired when a message is being run by an thread
   * @param id the message id
   * @param thread_to is the receiving thread index
   */
  probe thread__msg__pop(uint32_t id, uintptr_t thread);

  /**
   * Fired when actor is scheduled
   * @param scheduler is the scheduler that scheduled the actor
   * @param actor is the scheduled actor
   */
  probe actor__scheduled(uintptr_t scheduler, uintptr_t actor);

  /**
   * Fired when actor is descheduled
   * @param scheduler is the scheduler that descheduled the actor
   * @param actor is the descheduled actor
   */
  probe actor__descheduled(uintptr_t scheduler, uintptr_t actor);

  /**
   * Fired when actor becomes overloaded
   * @param actor is the overloaded actor
   */
  probe actor__overloaded(uintptr_t actor);

  /**
   * Fired when actor stops being overloaded
   * @param actor is the no longer overloaded actor
   */
  probe actor__overloaded__cleared(uintptr_t actor);

  /**
   * Fired when actor is under pressure
   * @param actor is the under pressure actor
   */
  probe actor__under__pressure(uintptr_t actor);

  /**
   * Fired when actor is no longer under pressure
   * @param actor is the no longer under pressure actor
   */
  probe actor__pressure__released(uintptr_t actor);

  /**
   * Fired when actor is muted
   * @param actor is the muted actor
   */
  probe actor__muted(uintptr_t actor);

  /**
   * Fired when actor is no longer muted
   * @param actor is the no longer muted actor
   */
  probe actor__unmuted(uintptr_t actor);

  /**
   * Fired when cpu goes into nanosleep
   * @param ns is nano seconds spent in sleep
   */
  probe cpu__nanosleep(uint64_t ns);

  /**
   * Fired when the garbage collection function is ending
   */
  probe gc__end(uintptr_t scheduler);

  /**
   * Fired when the garbage collector finishes sending an object
   */
  probe gc__send__end(uintptr_t scheduler);

  /**
   * Fired when the garbage collector stats sending an object
   */
  probe gc__send__start(uintptr_t scheduler);

  /**
   * Fired when the garbage collector finishes receiving an object
   */
  probe gc__recv__end(uintptr_t scheduler);

  /**
   * Fired when the garbage collector starts receiving an object
   */
  probe gc__recv__start(uintptr_t scheduler);

  /**
   * Fired when the garbage collection function has started
   */
  probe gc__start(uintptr_t scheduler);

  /**
   * Fired when the garbage collection threshold is changed with a certain factor
   * @param factor the factor with which the GC threshold is changed
   */
  probe gc__threshold(double factor);

  /**
   * Fired when memory is allocated on the heap
   * @param size the size of the allocated memory
   */
  probe heap__alloc(uintptr_t scheduler, unsigned long size);

  /**
   * Fired when runtime initiates
   */
  probe rt__init();

  /**
   * Fired when runtime is initiated and the program starts
   */
  probe rt__start();

  /**
   * Fired when runtime shutdown is started
   */
  probe rt__end();

  /**
   * Fired when a scheduler successfully steals a job
   * @param scheduler is the scheduler that stole the job
   * @param victim is the victim that the scheduler stole from
   * @param actor is actor that was stolen from the victim
   */
  probe work__steal__successful(uintptr_t scheduler, uintptr_t victim, uintptr_t actor);

  /**
   * Fired when a scheduler fails to steal a job
   * @param scheduler is the scheduler that attempted theft
   * @param victim is the victim that the scheduler attempted to steal from
   */
  probe work__steal__failure(uintptr_t scheduler, uintptr_t victim);

  /**
   * Fired when a scheduler suspends
   * @param scheduler is the scheduler that suspended
   */
  probe thread__suspend(uintptr_t scheduler);

  /**
   * Fired when a scheduler resumes
   * @param scheduler is the scheduler that resumed
   */
  probe thread__resume(uintptr_t scheduler);

};
