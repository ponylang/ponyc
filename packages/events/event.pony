interface EventNotify tag
  be _event_notify(event: Pointer[Event] tag, flags: U32)

primitive Event
  fun tag timer(owner: EventNotify, event: Pointer[Event] tag, nsec: U64):
    Pointer[Event] tag
  =>
    """
    Create or reset a timer. It will fire in nsec nanoseconds. It will not
    fire again unless it is armed again. If it is already armed, the new time
    replaces the old time. If nsec is -1, the timer is disarmed. The returned
    event pointer replaces the one passed in.
    """
    if not event.is_null() then
      @asio_event_unsubscribe[None](event)
    end

    if nsec != -1 then
      @asio_event_create[Pointer[Event]](owner, nsec, U32(4), true)
    else
      Pointer[Event]
    end

  fun tag socket(owner: EventNotify, fd: U32): Pointer[Event] tag =>
    """
    Create a socket event for a file descriptor.
    """
    @asio_event_create[Pointer[Event]](owner, fd.u64(), U32(3), true)

  fun tag fd(event: Pointer[Event] tag): U32 =>
    """
    Returns the file descriptor associated with an event.
    """
    if not event.is_null() then
      @asio_event_data[U64](event).u32()
    else
      0
    end

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

  fun tag unsubscribe(event: Pointer[Event] tag) =>
    """
    Unsubscribes an event.
    """
    if not event.is_null() then
      @asio_event_unsubscribe[None](event)
    end

  fun tag dispose(event: Pointer[Event] tag): Pointer[Event] tag =>
    """
    Disposes of an event.
    """
    if not event.is_null() then
      @asio_event_destroy[None](event)
      Pointer[Event]
    else
      event
    end
