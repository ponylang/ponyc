interface SignalNotify
  """
  Notifications for a signal.
  """
  fun ref apply(count: U32): Bool =>
    """
    Called with the the number of times the signal has fired since this was
    last called. Return false to stop listening for the signal.
    """
    true

  fun ref dispose() =>
    """
    Called if the signal is disposed. This is also called if the notifier
    returns false.
    """
    None

primitive SignalRaise
  """
  Raise a signal.
  """
  fun apply(sig: U32) =>
    ifdef osx then
      // On Darwin, @raise delivers the signal to the current thread, not the
      // process, but kqueue EVFILT_SIGNAL will only see signals delivered to
      // the process. @kill delivers the signal to a specific process.
      @kill[I32](@getpid[I32](), sig)
    else
      @raise[I32](sig)
    end
