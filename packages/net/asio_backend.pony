trait ref AsioBackend
  """
  The ASIO event operations a connection or listener needs from the
  runtime. `RuntimeAsio` is the production implementation; test code
  can substitute a fake to intercept event subscriptions, timers, and
  readiness state without real ASIO events.
  """
  new create()

  fun ref create_event(the_actor: AsioEventNotify, fd: U32): AsioEventID
    """
    Create a one-shot read/write ASIO event for `fd`, subscribed to
    `the_actor`.
    """

  fun ref destroy(event: AsioEventID)
    """
    Destroy an ASIO event after it has been disposed.
    """

  fun ref unsubscribe(event: AsioEventID)
    """
    Unsubscribe an ASIO event so no further notifications are
    delivered.
    """

  fun ref resubscribe_read(event: AsioEventID)
    """
    Re-arm the read side of a one-shot event.
    """

  fun ref resubscribe_write(event: AsioEventID)
    """
    Re-arm the write side of a one-shot event.
    """

  fun ref create_timer_event(the_actor: AsioEventNotify, nsec: U64)
    : AsioEventID
    """
    Create a timer ASIO event that fires after `nsec` nanoseconds.
    """

  fun ref set_timer(event: AsioEventID, nsec: U64)
    """
    Reset a timer event to fire after `nsec` nanoseconds.
    """

  fun ref set_readable(event: AsioEventID)
    """
    Mark the event as readable.
    """

  fun ref set_unreadable(event: AsioEventID)
    """
    Mark the event as not readable.
    """

  fun ref set_writeable(event: AsioEventID)
    """
    Mark the event as writeable.
    """

  fun ref set_unwriteable(event: AsioEventID)
    """
    Mark the event as not writeable.
    """

  fun ref event_fd(event: AsioEventID): U32
    """
    Return the file descriptor stored in an ASIO event.
    """

  fun ref get_disposable(event: AsioEventID): Bool
    """
    True when the event has been marked disposable by the runtime.
    """
