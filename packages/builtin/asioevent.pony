type AsioEventID is Pointer[AsioEvent] tag

interface AsioEventNotify tag
  be _event_notify(event: AsioEventID, flags: U32, arg: U64)

primitive AsioEvent
  """
  Functions for asynchronous event notification.
  """
  fun none(): AsioEventID =>
    """
    An empty event.
    """
    AsioEventID

  fun readable(flags: U32): Bool =>
    """
    Returns true if the flags contain the readable flag.
    """
    (flags and (1 << 0)) != 0

  fun writeable(flags: U32): Bool =>
    """
    Returns true if the flags contain the writeable flag.
    """
    (flags and (1 << 1)) != 0

  fun disposable(flags: U32): Bool =>
    """
    Returns true if the event should be disposed of.
    """
    flags == 0
