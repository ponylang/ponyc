use "collections"

class TCPListener[TCP: TCPBackend ref = RuntimeBackend]
  """
  The TCP listener: opens a listening socket, runs the accept loop, and
  enforces the connection limit. A `TCPListenerActor` owns one and delegates to
  it. Create it with `TCPListener(auth, host, port, this)`, using
  `TCPListener.none()` as the field initializer before that.
  """
  var _tcp: TCP = TCP
  let _host: String
  let _port: String
  let _limit: (MaxSpawn | None)
  let _ip_version: IPVersion
  var _open_connections: U32 = 0
  var _paused: Bool = false
  var _event: AsioEventID = AsioEvent.none()
  var _fd: U32 = -1
  var _listening: Bool = false
  var _enclosing: (TCPListenerActor[TCP] ref | None)

  new create(auth: TCPListenAuth,
    host: String,
    port: String,
    enclosing: TCPListenerActor[TCP] ref,
    ip_version: IPVersion = DualStack,
    limit: (MaxSpawn | None) = DefaultMaxSpawn())
  =>
    _host = host
    _port = port
    _ip_version = ip_version
    _limit = limit
    _enclosing = enclosing
    enclosing._finish_initialization()

  new none() =>
    """
    A placeholder listener for the actor's field before real initialization,
    replaced by a `create` listener once the actor starts.
    """
    _host = ""
    _port = ""
    _limit = None
    _ip_version = DualStack
    _enclosing = None

  fun ref close() =>
    match \exhaustive\ _enclosing
    | let e: TCPListenerActor[TCP] ref =>
      // TODO: when in debug mode we should blow up if listener is closed
      if _listening then
        _listening = false

        if not _event.is_null() then
          PonyAsio.unsubscribe(_event)
          // POSIX closes the listener fd here. On Windows the readiness backend
          // owns the close: it happens when the deferred
          // ProcessSocketNotifications REMOVE from the unsubscribe above is
          // seen, so closing here would strand the disposal handshake. The
          // accepted/rejected fds in _accept are raw (never subscribed), so
          // those closes stay cross-platform.
          ifdef not windows then
            _tcp.close(_fd)
          end
          _fd = -1
          e._on_closed()
        end
      end
    | None =>
      _Unreachable()
    end

  fun ref local_address(): NetAddress =>
    """
    Return the local IP address. If this TCPListener is closed then the
    address returned is invalid.
    """
    let ip = recover NetAddress end
    _tcp.sockname(_fd, ip)
    ip

  fun ref _event_notify(event: AsioEventID, flags: U32) =>
    if event isnt _event then
      return
    end

    if AsioEvent.errored(flags) then
      close()
      return
    end

    if AsioEvent.readable(flags) then
      _accept()
    end

    if AsioEvent.disposable(flags) then
      PonyAsio.destroy(_event)
      _event = AsioEvent.none()
      _listening = false
    end

  fun ref _accept() =>
    match \exhaustive\ _enclosing
    | let e: TCPListenerActor[TCP] ref =>
      if _listening then
        while not _at_connection_limit() do
          var fd = _tcp.accept(_event)

          // 0: would block, -1: error
          if fd <= 0 then
            return
          end

          try
            let opened = e._on_accept(fd.u32())?
            opened._register_spawner(e)
            _open_connections = _open_connections + 1
          else
            // Rejected before an event was created — raw fd, close on both
            // platforms.
            _tcp.close(fd.u32())
          end
        end

        _paused = true
      else
        // It's possible that after closing, we got an event for a connection
        // attempt. If the listener is not open, do not start a new connection.
        return
      end
    | None =>
      _Unreachable()
    end

  fun _at_connection_limit(): Bool =>
    match \exhaustive\ _limit
    | let l: MaxSpawn => _open_connections >= l()
    | None => false
    end

  fun ref _connection_closed() =>
    _open_connections = _open_connections - 1
    if _paused and not _at_connection_limit() then
      _paused = false
      _accept()
    end

  fun ref _finish_initialization() =>
    match \exhaustive\ _enclosing
    | let e: TCPListenerActor[TCP] ref =>
      _event = _tcp.listen(e, _host, _port where ip_version = _ip_version)
      if not _event.is_null() then
        _fd = PonyAsio.event_fd(_event)
        _listening = true
        e._on_listening()
      else
        e._on_listen_failure()
      end
    | None =>
      _Unreachable()
    end
