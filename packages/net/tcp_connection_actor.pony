trait tag TCPConnectionActor[TCP: TCPBackend ref = RuntimeBackend]
  is AsioEventNotify
  """
  The actor trait a connection actor implements. Provide `_connection()`
  returning the `TCPConnection` the actor owns; the behaviors here deliver ASIO
  events, resumed reads, and failed-send notifications to it. Implement this
  together with a `ClientLifecycleEventReceiver` or
  `ServerLifecycleEventReceiver` for the connection callbacks.
  """
  fun ref _connection(): TCPConnection[TCP]

  be dispose() =>
    """
    Close connection
    """
    // hard_close() — disposal is unconditional teardown, not graceful shutdown.
    // See #229 for the edge-triggered race that makes close() unreliable here.
    _connection().hard_close()

  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    // `arg` carried the IOCP completion byte count under the old Windows
    // backend. With readiness notifications it is unused on every platform.
    _connection()._event_notify(event, flags)

  be _read_again() =>
    """
    Resume reading: re-enter the read loop, which processes any buffered data
    and then reads from the socket. Same on every platform.
    """
    _connection().read_again()

  be _register_spawner(listener: TCPListenerActor[TCP]) =>
    """
    Register the listener as the spawner of this connection
    """
    _connection()._register_spawner(listener)

  be _notify_send_failed(token: SendToken) =>
    """
    Deferred delivery of _on_send_failed to the lifecycle event receiver.
    """
    _connection()._fire_on_send_failed(token)

  be _finish_initialization() =>
    _connection()._finish_initialization()
