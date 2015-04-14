actor TCPListener
  """
  Listens for new network connections.
  """
  var _notify: TCPListenNotify
  var _fd: U64
  var _event: EventID = Event.none()
  var _closed: Bool = false

  new create(notify: TCPListenNotify iso, host: String = "",
    service: String = "0")
  =>
    """
    Listens for both IPv4 and IPv6 connections.
    """
    _notify = consume notify
    _fd = @os_listen_tcp[U64](this, host.cstring(), service.cstring())
    _notify_listening()

  new ip4(notify: TCPListenNotify iso, host: String = "",
    service: String = "0")
  =>
    """
    Listens for IPv4 connections.
    """
    _notify = consume notify
    _fd = @os_listen_tcp4[U64](this, host.cstring(), service.cstring())
    _notify_listening()

  new ip6(notify: TCPListenNotify iso, host: String = "",
    service: String = "0")
  =>
    """
    Listens for IPv6 connections.
    """
    _notify = consume notify
    _fd = @os_listen_tcp6[U64](this, host.cstring(), service.cstring())
    _notify_listening()

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
    @os_sockname[None](_fd, ip)
    ip

  fun ref set_notify(notify: TCPListenNotify) =>
    """
    Change the notifier.
    """
    _notify = notify

  be _event_notify(event: EventID, flags: U32, arg: U64) =>
    """
    When we are readable, we accept new connections until none remain.
    """
    if not _closed then
      _event = event

      if Event.readable(flags) then
        var newfd = arg

        while true do
          var fd = @os_accept[U64](_event, newfd)
          newfd = 0

          if fd == -1 then
            break
          end

          try
            TCPConnection._accept(_notify.connected(this), fd)
          else
            @os_closesocket[None](fd)
          end
        end
      end
    else
      if Platform.windows() and Event.readable(flags) then
        // On windows, the unsubscribe is done after the asynchronous accept is
        // cancelled.
        @asio_event_unsubscribe[None](_event)
      end
    end

    if Event.disposable(flags) then
      @asio_event_destroy[None](event)
      _event = Event.none()
      _notify.closed(this)
    end

  fun ref _notify_listening() =>
    """
    Inform the notifier that we're listening.
    """
    if _fd != -1 then
      _notify.listening(this)
    else
      _notify.not_listening(this)
    end

  fun ref close() =>
    """
    Dispose of resources.
    """
    _closed = true

    if _fd != -1 then
      @os_closesocket[None](_fd)
      _fd = -1
    end

    // When not on windows, the unsubscribe is done immediately.
    if not Platform.windows() then
      @asio_event_unsubscribe[None](_event)
    end
