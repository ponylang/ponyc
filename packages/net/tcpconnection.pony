actor TCPConnection
  """
  A TCP connection.
  """
  var _fd: U32
  var _event: Pointer[_Event]
  var _connected: Bool = false
  var _readable: Bool = false
  var _writeable: Bool = false

  // TODO: programmatically determine behaviour IDs
  new create(host: String, service: String) =>
    """
    Connect via IPv4 or IPv6.
    """
    _fd = @os_connect_tcp[U32](host.cstring(), service.cstring())
    _event = _Event.socket(_fd, 4)

  new ip4(host: String, service: String) =>
    """
    Connect via IPv4.
    """
    _fd = @os_connect_tcp4[U32](host.cstring(), service.cstring())
    _event = _Event.socket(_fd, 4)

  new ip6(host: String, service: String) =>
    """
    Connect via IPv6.
    """
    _fd = @os_connect_tcp6[U32](host.cstring(), service.cstring())
    _event = _Event.socket(_fd, 4)

  new _accepted(fd: U32) =>
    """
    A new connection accepted on a server.
    """
    _fd = fd
    _event = _Event.socket(_fd, 4)

  be _handle_event(event: Pointer[_Event] tag, flags: U32) =>
    if _Event.writeable(flags) then
      if not _connected then
        if not @os_connected[Bool](_fd) then
          // TODO: notify of a failed connection?
          return _final()
        end

        _connected = true
      end

      _writeable = true

      // TODO: flush pending writes
    end

    if _Event.readable(flags) then
      // TODO: read, but not everything, since we would starve others
      // who do we notify?
      None
    end

  be write(data: Bytes val) =>
    // TODO: write to the socket if we can, store for later otherwise
    None

  be dispose() =>
    """
    Close the connection.
    """
    _final()

  fun ref _final() =>
    """
    Dispose of resources.
    """
    _event = _Event.dispose(_event)

    if _fd != -1 then
      @close[I32](_fd)
      _fd = -1
    end
