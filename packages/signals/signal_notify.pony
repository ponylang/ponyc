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
    @raise[I32](sig)
