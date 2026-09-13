class \nodoc\ _RecordingAsio is AsioBackend
  """
  An AsioBackend that delegates to RuntimeAsio but records timer
  operations. Tests read back the recorded calls to verify that
  TCPConnection routes timer creation and resets through the ASIO
  backend.
  """
  let _inner: RuntimeAsio = RuntimeAsio
  var set_timer_count: USize = 0
  var create_timer_count: USize = 0
  var unsubscribe_count: USize = 0

  new create() => None

  fun ref create_event(the_actor: AsioEventNotify, fd: U32): AsioEventID =>
    _inner.create_event(the_actor, fd)

  fun ref destroy(event: AsioEventID) =>
    _inner.destroy(event)

  fun ref unsubscribe(event: AsioEventID) =>
    unsubscribe_count = unsubscribe_count + 1
    _inner.unsubscribe(event)

  fun ref resubscribe_read(event: AsioEventID) =>
    _inner.resubscribe_read(event)

  fun ref resubscribe_write(event: AsioEventID) =>
    _inner.resubscribe_write(event)

  fun ref create_timer_event(the_actor: AsioEventNotify, nsec: U64)
    : AsioEventID
  =>
    create_timer_count = create_timer_count + 1
    _inner.create_timer_event(the_actor, nsec)

  fun ref set_timer(event: AsioEventID, nsec: U64) =>
    set_timer_count = set_timer_count + 1
    _inner.set_timer(event, nsec)

  fun ref set_readable(event: AsioEventID) =>
    _inner.set_readable(event)

  fun ref set_unreadable(event: AsioEventID) =>
    _inner.set_unreadable(event)

  fun ref set_writeable(event: AsioEventID) =>
    _inner.set_writeable(event)

  fun ref set_unwriteable(event: AsioEventID) =>
    _inner.set_unwriteable(event)

  fun ref event_fd(event: AsioEventID): U32 =>
    _inner.event_fd(event)

  fun ref get_disposable(event: AsioEventID): Bool =>
    _inner.get_disposable(event)
