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

  fun _stdin(owner: EventNotify): EventID =>
    """
    Create a socket event for a file descriptor.
    """
    @asio_event_create[Pointer[Event]](owner, U64(0), U32(1), true)

  fun timer(owner: EventNotify, event: EventID, nsec: U64): EventID =>
    """
    Create or reset a timer. It will fire in nsec nanoseconds. It will not
    fire again unless it is armed again. If it is already armed, the new time
    replaces the old time. If nsec is -1, the timer is disarmed. The returned
    event pointer replaces the one passed in.
    """
    if not event.is_null() then
      if nsec != -1 then
        @asio_event_update[None](event, nsec)
        event
      else
        @asio_event_unsubscribe[None](event)
        none()
      end
    else
      if nsec != -1 then
        @asio_event_create[Pointer[Event]](owner, nsec, U32(4), true)
      else
        none()
      end
    end

  fun socket(owner: EventNotify, fd: U32): EventID =>
    """
    Create a socket event for a file descriptor.
    """
    @asio_event_create[Pointer[Event]](owner, fd.u64(), U32(3), true)

  fun fd(event: EventID): U32 =>
    """
    Returns the file descriptor associated with an event.
    """
    if not event.is_null() then
      @asio_event_data[U64](event).u32()
    else
      0
    end

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

  fun unsubscribe(event: EventID) =>
    """
    Unsubscribes an event.
    """
    if not event.is_null() then
      @asio_event_unsubscribe[None](event)
    end

  fun dispose(event: EventID): EventID =>
    """
    Disposes of an event.
    """
    if not event.is_null() then
      @asio_event_destroy[None](event)
      none()
    else
      event
    end
