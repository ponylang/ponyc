use @pony_schedulers[U32]()
use @pony_active_schedulers[U32]()
use @pony_min_schedulers[U32]()
use @pony_scheduler_yield[Bool]()

primitive Scheduler
  """
  Provides functions that expose information about runtime schedulers.
  """

  fun schedulers(auth: RuntimeInfoAuth): U32 =>
    """
    Returns the maximum number of schedulers available to run actors.
    """
    @pony_schedulers()

  fun active_schedulers(auth: RuntimeInfoAuth): U32 =>
    """
    Returns the number of schedulers currently available to run actors.
    """
    @pony_active_schedulers()

  fun minimum_schedulers(auth: RuntimeInfoAuth): U32 =>
    """
    Returns the minimum number of schedulers. The active number of schedulers is
    guaranteed to never drop below this number.
    """
    @pony_min_schedulers()

  fun sleeping_schedulers(auth: RuntimeInfoAuth): U32 =>
    """
    Returns the number of schedulers that are currently sleeping and not
    available run actors. Schedulers are put to sleep if there isn't enough
    work to keep all of the possible schedulers busy.
    """
    @pony_schedulers() - @pony_active_schedulers()

  fun scaling_is_active(auth: RuntimeInfoAuth): Bool =>
    """
    Returns true is scheduler scaling is on and the number of active schedulers
    can change while the program is running based on load.
    """
    schedulers(auth) > minimum_schedulers(auth)

  fun will_yield_cpu(auth: RuntimeInfoAuth): Bool =>
    """
    Returns true if schedulers without work will yield the CPU allowing other
    processes to have access.
    """
    @pony_scheduler_yield()
