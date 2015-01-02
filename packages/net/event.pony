interface _EventNotify tag
  be _event_notify(event: Pointer[_Event] tag, flags: U32)

primitive _Event
  fun tag readable(flags: U32): Bool =>
    """
    Returns true if the flags contain the readable flag.
    """
    (flags and (1 << 0)) != 0

  fun tag writeable(flags: U32): Bool =>
    """
    Returns true if the flags contain the writeable flag.
    """
    (flags and (1 << 1)) != 0

  fun tag disposable(flags: U32): Bool =>
    """
    Returns true if the event should be disposed of.
    """
    flags == 0

  fun tag unsubscribe(event: Pointer[_Event] tag) =>
    """
    Unsubscribes an event.
    """
    if not event.is_null() then
      @asio_event_unsubscribe[None](event)
    end

  fun tag dispose(event: Pointer[_Event] tag): Pointer[_Event] tag =>
    """
    Disposes of an event.
    """
    if not event.is_null() then
      @asio_event_destroy[None](event)
      Pointer[_Event]
    else
      event
    end
