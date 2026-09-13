class RuntimeAsio is AsioBackend
  """
  Delegates every ASIO operation to the runtime's `pony_asio_event_*`
  functions. Production connections use this via the default type
  parameter on `TCPConnection`.
  """
  new create() => None

  fun create_event(the_actor: AsioEventNotify, fd: U32): AsioEventID =>
    PonyAsio.create_event(the_actor, fd)

  fun destroy(event: AsioEventID) =>
    PonyAsio.destroy(event)

  fun unsubscribe(event: AsioEventID) =>
    PonyAsio.unsubscribe(event)

  fun resubscribe_read(event: AsioEventID) =>
    PonyAsio.resubscribe_read(event)

  fun resubscribe_write(event: AsioEventID) =>
    PonyAsio.resubscribe_write(event)

  fun create_timer_event(the_actor: AsioEventNotify, nsec: U64)
    : AsioEventID
  =>
    PonyAsio.create_timer_event(the_actor, nsec)

  fun set_timer(event: AsioEventID, nsec: U64) =>
    PonyAsio.set_timer(event, nsec)

  fun set_readable(event: AsioEventID) =>
    PonyAsio.set_readable(event)

  fun set_unreadable(event: AsioEventID) =>
    PonyAsio.set_unreadable(event)

  fun set_writeable(event: AsioEventID) =>
    PonyAsio.set_writeable(event)

  fun set_unwriteable(event: AsioEventID) =>
    PonyAsio.set_unwriteable(event)

  fun event_fd(event: AsioEventID): U32 =>
    PonyAsio.event_fd(event)

  fun get_disposable(event: AsioEventID): Bool =>
    PonyAsio.get_disposable(event)
