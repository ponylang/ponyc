trait tag ConnectedUDPSocketActor[
  UDP: ConnectedUDPBackend ref = ConnectedUDPRuntimeBackend]
  is AsioEventNotify
  """
  The actor trait a connected UDP socket actor implements. Provide
  `_socket()` returning the `ConnectedUDPSocket` the actor owns; the
  behaviors here deliver ASIO events and resumed reads to it. Implement
  this together with `ConnectedUDPLifecycleEventReceiver` for the socket
  callbacks.
  """
  fun ref _socket(): ConnectedUDPSocket[UDP]

  be dispose() =>
    """
    Close the socket.
    """
    _socket().close()

  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    _socket()._event_notify(event, flags)

  be _read_again() =>
    """
    Resume reading: re-enter the read loop after yielding or exhausting the
    per-turn budget.
    """
    _socket().read_again()

  be _finish_initialization() =>
    _socket()._finish_initialization()
