provider pony {
  /**
   * Fired when the garbage collection function has started
   */
  probe gc__start();

  /**
   * Fired when the garbage collection function is ending
   */
  probe gc__end();
};
