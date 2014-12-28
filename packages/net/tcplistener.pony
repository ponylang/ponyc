actor TCPListener is Socket
  """
  Listens for new network connections.
  """
  var _notify: TCPListenNotify
  var _fd: U32
  var _event: Pointer[_Event] tag = Pointer[_Event]

  new create(notify: TCPListenNotify iso, host: String = "localhost",
    service: String = "0")
  =>
    """
    Listens for both IPv4 and IPv6 connections.
    """
    _notify = consume notify
    _fd = @os_listen_tcp[U32](this, host.cstring(), service.cstring())
    _notify_listening()

  new ip4(notify: TCPListenNotify iso, host: String = "localhost",
    service: String = "0")
  =>
    """
    Listens for IPv4 connections.
    """
    _notify = consume notify
    _fd = @os_listen_tcp4[U32](this, host.cstring(), service.cstring())
    _notify_listening()

  new ip6(notify: TCPListenNotify iso, host: String = "localhost",
    service: String = "0")
  =>
    """
    Listens for IPv6 connections.
    """
    _notify = consume notify
    _fd = @os_listen_tcp6[U32](this, host.cstring(), service.cstring())
    _notify_listening()

  fun ref set_notify(notify: TCPListenNotify) =>
    """
    Change the notifier.
    """
    _notify = notify

  be dispose() =>
    """
    Stop listening.
    """
    _final()

  fun box _get_fd(): U32 =>
    """
    Private call to get the file descriptor, used by the Socket trait.
    """
    _fd

  be _event_notify(event: Pointer[_Event] tag, flags: U32) =>
    """
    When we are readable, we accept new connections until none remain.
    """
    _event = event

    if _Event.readable(flags) then
      while true do
        var s = @os_accept[U32](_fd)

        if s == -1 then
          break
        end

        TCPConnection._accept(_notify.connection(this), s)
      end
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

  fun ref _final() =>
    """
    Dispose of resources.
    """
    _event = _Event.dispose(_event)

    if _fd != -1 then
      @os_closesocket[None](_fd)
      _fd = -1
    end
