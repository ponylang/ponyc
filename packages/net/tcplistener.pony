actor TCPListener
  """
  Listens for new network connections.
  """
  var _notify: TCPListenNotify
  var _fd: U32
  var _event: AsioEventID = AsioEvent.none()
  var _closed: Bool = false
  let _limit: USize
  var _count: USize = 0
  var _paused: Bool = false

  new create(notify: TCPListenNotify iso, host: String = "",
    service: String = "0", limit: USize = 0)
  =>
    """
    Listens for both IPv4 and IPv6 connections.
    """
    _limit = limit
    _notify = consume notify
    _event = @os_listen_tcp[AsioEventID](this, host.cstring(), service.cstring())
    _fd = @asio_event_fd(_event)
    _notify_listening()

  new ip4(notify: TCPListenNotify iso, host: String = "",
    service: String = "0", limit: USize = 0)
  =>
    """
    Listens for IPv4 connections.
    """
    _limit = limit
    _notify = consume notify
    _event = @os_listen_tcp4[AsioEventID](this, host.cstring(),
      service.cstring())
    _fd = @asio_event_fd(_event)
    _notify_listening()

  new ip6(notify: TCPListenNotify iso, host: String = "",
    service: String = "0", limit: USize = 0)
  =>
    """
    Listens for IPv6 connections.
    """
    _limit = limit
    _notify = consume notify
    _event = @os_listen_tcp6[AsioEventID](this, host.cstring(),
      service.cstring())
    _fd = @asio_event_fd(_event)
    _notify_listening()

  be set_notify(notify: TCPListenNotify iso) =>
    """
    Change the notifier.
    """
    _notify = consume notify

  be dispose() =>
    """
    Stop listening.
    """
    close()

  fun local_address(): IPAddress =>
    """
    Return the bound IP address.
    """
    let ip = recover IPAddress end
    @os_sockname[Bool](_fd, ip)
    ip

  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    """
    When we are readable, we accept new connections until none remain.
    """
    if event isnt _event then
      return
    end

    if AsioEvent.readable(flags) then
      _accept(arg)
    end

    if AsioEvent.disposable(flags) then
      @asio_event_destroy(_event)
      _event = AsioEvent.none()
    end

  be _conn_closed() =>
    """
    An accepted connection has closed. If we have dropped below the limit, try
    to accept new connections.
    """
    _count = _count - 1

    if _paused and (_count < _limit) then
      _paused = false
      _accept()
    end

  fun ref _accept(ns: U32 = 0) =>
    """
    Accept connections as long as we have spawned fewer than our limit.
    """
    ifdef windows then
      if ns == -1 then
        // Unsubscribe when we get an invalid socket in the event.
        @asio_event_unsubscribe(_event)
        return
      end

      if ns > 0 then
        if _closed then
          @os_closesocket[None](ns)
          return
        end

        _spawn(ns)
      end

      // Queue an accept if we're not at the limit.
      if (_limit == 0) or (_count < _limit) then
        @os_accept[U32](_event)
      else
        _paused = true
      end
    else
      if _closed then
        return
      end

      while (_limit == 0) or (_count < _limit) do
        var fd = @os_accept[U32](_event)

        match fd
        | -1 =>
          // Something other than EWOULDBLOCK, try again.
          None
        | 0 =>
          // EWOULDBLOCK, don't try again.
          return
        else
          _spawn(fd)
        end
      end

      _paused = true
    end

  fun ref _spawn(ns: U32) =>
    """
    Spawn a new connection.
    """
    try
      TCPConnection._accept(this, _notify.connected(this), ns)
      _count = _count + 1
    else
      @os_closesocket[None](ns)
    end

  fun ref _notify_listening() =>
    """
    Inform the notifier that we're listening.
    """
    if not _event.is_null() then
      _notify.listening(this)
    else
      _closed = true
      _notify.not_listening(this)
    end

  fun ref close() =>
    """
    Dispose of resources.
    """
    if _closed then
      return
    end

    _closed = true

    if not _event.is_null() then
      @os_closesocket[None](_fd)
      _fd = -1

      // When not on windows, the unsubscribe is done immediately.
      ifdef not windows then
        @asio_event_unsubscribe(_event)
      end

      _notify.closed(this)
    end
