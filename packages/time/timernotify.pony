interface TimerNotify
  """
  Notifications for timer.
  """
  fun ref apply(timer: Timer, count: U64): Bool =>
    """
    Called with the the number of times the timer has fired since this was last
    called. Return true to reschedule the timer (if it has an interval), or
    false to cancel the timer (even if it has an interval).
    """
    true

  fun ref cancel(timer: Timer) =>
    """
    Called if the timer is cancelled. This is also called if the notifier
    returns false.
    """
    None
