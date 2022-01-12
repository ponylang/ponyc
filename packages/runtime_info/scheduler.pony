use @pony_schedulers[U32]()
use @pony_active_schedulers[U32]()

primitive Scheduler
  fun schedulers(): U32 =>
    @pony_schedulers()

  fun active_schedulers(): U32 =>
    @pony_active_schedulers()
