provider pony {
  /**
   * Fired when a actor is being created
   */
  probe actor__alloc();

  /**
   * Fired when a message is being send
   * @param id the message id
   */
  probe actor__msg__send(uint32_t id);

  /**
   * Fired when the garbage collection function is ending
   */
  probe gc__end();

  /**
   * Fired when the garbage collector finishes sending an object
   */
  probe gc__send__end();

  /**
   * Fired when the garbage collector stats sending an object
   */
  probe gc__send__start();

  /**
   * Fired when the garbage collector finishes receiving an object
   */
  probe gc__recv__end();

  /**
   * Fired when the garbage collector starts receiving an object
   */
  probe gc__recv__start();

  /**
   * Fired when the garbage collection function has started
   */
  probe gc__start();

  /**
   * Fired when memory is allocated on the heap
   * @param size the size of the allocated memory
   */
  probe heap__alloc(unsigned long size);
};
