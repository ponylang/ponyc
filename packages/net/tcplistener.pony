use "events"

actor TCPListener
  """
  Listens for new network connections.
  """
  var _notify: TCPListenNotify
  var _fd: U32
  var _event: Pointer[Event] tag = Pointer[Event]
  var _closed: Bool = false

  new create(notify: TCPListenNotify iso, host: String = "",
    service: String = "0")
  =>
    """
    Listens for both IPv4 and IPv6 connections.
    """
    _notify = consume notify
    _fd = @os_listen_tcp[U32](this, host.cstring(), service.cstring())
    _notify_listening()

  new ip4(notify: TCPListenNotify iso, host: String = "",
    service: String = "0")
  =>
    """
    Listens for IPv4 connections.
    """
    _notify = consume notify
    _fd = @os_listen_tcp4[U32](this, host.cstring(), service.cstring())
    _notify_listening()

  new ip6(notify: TCPListenNotify iso, host: String = "",
    service: String = "0")
  =>
    """
    Listens for IPv6 connections.
    """
    _notify = consume notify
    _fd = @os_listen_tcp6[U32](this, host.cstring(), service.cstring())
    _notify_listening()

  be dispose() =>
    """
    Stop listening.
    """
    _close()

  fun box local_address(): IPAddress =>
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

  be _event_notify(event: Pointer[Event] tag, flags: U32) =>
    """
    When we are readable, we accept new connections until none remain.
    """
    if not _closed then
      _event = event

      if Event.readable(flags) then
        while true do
          var fd = @os_accept[U32](_fd)

          if fd == -1 then
            break
          end

          TCPConnection._accept(_notify.connected(this), fd)
        end
      end
    end

    if Event.disposable(flags) then
      _event = Event.dispose(event)
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

  fun ref _close() =>
    """
    Dispose of resources.
    """
    Event.unsubscribe(_event)
    _closed = true

    if _fd != -1 then
      @os_closesocket[None](_fd)
      _fd = -1
    end
