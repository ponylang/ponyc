interface TCPNewConnection val
  """
  Specifies what to do when a new connection is accepted.
  """
  fun val apply(connection: TCPConnection)

interface TCPBindNotify val
  """
  Retrieve a bound host and service.
  """
  fun val apply(host: String, service: String)

actor TCPListener
  """
  Listens for new network connections.
  """
  var _handler: TCPNewConnection
  var _fd: U32
  var _event: Pointer[_Event]

  // TODO: programmatically determine behaviour IDs
  new create(handler: TCPNewConnection, host: String = "", service: String = "")
  =>
    """
    Listens for both IPv4 and IPv6 connections.
    """
    _handler = handler
    _fd = @os_listen_tcp[U32](host.cstring(), service.cstring())
    _event = _Event.socket(_fd, 3)

  new ip4(handler: TCPNewConnection, host: String = "", service: String = "") =>
    """
    Listens for IPv4 connections.
    """
    _handler = handler
    _fd = @os_listen_tcp4[U32](host.cstring(), service.cstring())
    _event = _Event.socket(_fd, 3)

  new ip6(handler: TCPNewConnection, host: String = "", service: String = "") =>
    """
    Listens for IPv6 connections.
    """
    _handler = handler
    _fd = @os_listen_tcp6[U32](host.cstring(), service.cstring())
    _event = _Event.socket(_fd, 3)

  be _handle_event(event: Pointer[_Event] tag, flags: U32) =>
    """
    When we are readable, we accept new connections until none remain.
    """
    if _Event.readable(flags) then
      while true do
        var s = @os_accept[U32](_fd)

        if s == -1 then
          break None
        end

        _handler(TCPConnection._accepted(s))
      end
    end

  be set_handler(handler: TCPNewConnection) =>
    """
    Change the handler.
    """
    _handler = handler

  be local_bind(notify: TCPBindNotify) =>
    // TODO: get the local name
    None

  be dispose() =>
    """
    Stop listening.
    """
    _final()

  fun ref _final() =>
    """
    Dispose of resources.
    """
    _event = _Event.dispose(_event)

    if _fd != -1 then
      @os_closesocket[I32](_fd)
      _fd = -1
    end
