trait \nodoc\ tag _AsioEventInjectable
  """
  Test-only injection point for synthetic ASIO events. Mock
  `AsioBackend` classes call `_inject_asio_event` to deliver events to
  the connection actor, working around the fact that
  `AsioEventNotify._event_notify` is a behavior in `builtin` and
  cannot be called on an actor tag from `net`.

  The behavior is asynchronous (`be`, not `fun`) so injected events
  arrive in a future turn, matching production ASIO event delivery.
  """
  be _inject_asio_event(event: AsioEventID, flags: U32)
