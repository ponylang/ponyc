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

  fun tag receive(event: Pointer[_Event] tag) =>
    """
    This must be called when an event is received.
    """
    @asio_event_recv[None](event)

  fun tag dispose(event: Pointer[_Event] tag): Pointer[_Event] tag =>
    """
    Disposes of an event notification.
    """
    if not event.is_null() then
      @asio_event_unsubscribe[None](event)
      Pointer[_Event]
    else
      event
    end
