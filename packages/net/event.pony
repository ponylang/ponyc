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

  fun tag socket(e: _EventNotify, fd: U32): Pointer[_Event] =>
    """
    Creates a socket event notification.
    """
    if fd != -1 then
      @os_socket_event[Pointer[_Event]](e, fd)
    else
      Pointer[_Event]
    end

  fun tag dispose(event: Pointer[_Event]): Pointer[_Event] =>
    """
    Disposes of an event notification.
    """
    if not event.is_null() then
      @asio_event_unsubscribe[None](event)
      Pointer[_Event]
    else
      event
    end
