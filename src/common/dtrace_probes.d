provider pony {
  /**
   * Fired when a actor is being created
   */
  probe actor__alloc(uintptr_t scheduler);

  /**
   * Fired when a message is being send
   * @param id the message id
   */
  probe actor__msg__send(uintptr_t scheduler, uint32_t id);

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
   * Fired when memory is allocated on the heap
   * @param size the size of the allocated memory
   */
  probe heap__alloc(uintptr_t scheduler, unsigned long size);
};
