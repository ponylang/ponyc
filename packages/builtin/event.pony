type EventID is Pointer[Event] tag

interface EventNotify tag
  be _event_notify(event: EventID, flags: U32)

primitive Event
  """
  Functions for asynchronous event notification.
  """
  fun none(): EventID =>
    """
    An empty event.
    """
    Pointer[Event]

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
