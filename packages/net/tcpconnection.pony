use "collections"

actor TCPConnection is Socket
  """
  A TCP connection. When connecting, the Happy Eyeballs algorithm is used.
  """
  var _notify: TCPConnectionNotify
  var _connect_count: U32
  var _fd: U32 = -1
  var _event: Pointer[_Event] tag = Pointer[_Event]
  var _connected: Bool = false
  var _readable: Bool = false
  var _writeable: Bool = false
  var _closing: Bool = false
  var _pending: List[_TCPPendingWrite] = List[_TCPPendingWrite]
  var _last_read: U64 = 64

  new create(notify: TCPConnectionNotify iso, host: String, service: String) =>
    """
    Connect via IPv4 or IPv6.
    """
    _notify = consume notify
    _connect_count = @os_connect_tcp[U32](this, host.cstring(),
      service.cstring())
    _notify_connecting()

  new ip4(notify: TCPConnectionNotify iso, host: String, service: String) =>
    """
    Connect via IPv4.
    """
    _notify = consume notify
    _connect_count = @os_connect_tcp4[U32](this, host.cstring(),
      service.cstring())
    _notify_connecting()

  new ip6(notify: TCPConnectionNotify iso, host: String, service: String) =>
    """
    Connect via IPv6.
    """
    _notify = consume notify
    _connect_count = @os_connect_tcp6[U32](this, host.cstring(),
      service.cstring())
    _notify_connecting()

  new _accept(notify: TCPConnectionNotify iso, fd: U32) =>
    """
    A new connection accepted on a server.
    """
    _notify = consume notify
    _connect_count = 0
    _fd = fd
    _event = @os_socket_event[Pointer[_Event]](this, fd)
    _connected = true
    _notify.accepted(this)

  be write(data: Bytes val) =>
    """
    Write as much as possible to the socket. Set _writeable to false if not
    everything was written. On an error, dispose of the connection.
    """
    if _writeable then
      try
        var len = @os_send[U64](_fd, data.cstring(), data.size()) ?

        if len < data.size() then
          _pending.append(_TCPPendingWrite(data, len))
          _writeable = false
        end
      else
        _close()
      end
    elseif not _closing then
      _pending.append(_TCPPendingWrite(data, 0))
    end

  be dispose() =>
    """
    Close the connection once all writes are sent.
    """
    _closing = true

    if (_connect_count == 0) and (_pending.size() == 0) then
      _close()
    end

  fun ref set_notify(notify: TCPConnectionNotify ref) =>
    """
    Change the notifier.
    """
    _notify = notify

  fun ref set_nodelay(state: Bool) =>
    """
    Turn Nagle on/off. Defaults to on. This can only be set on a connected
    socket.
    """
    if _connected then
      @os_nodelay[None](_fd, state)
    end

  fun ref set_keepalive(secs: U32) =>
    """
    Sets the TCP keepalive timeout to approximately secs seconds. Exact timing
    is OS dependent. If secs is zero, TCP keepalive is disabled. TCP keepalive
    is disabled by default. This can only be set on a connected socket.
    """
    if _connected then
      @os_keepalive[None](_fd, secs)
    end

  fun box _get_fd(): U32 =>
    """
    Private call to get the file descriptor, used by the Socket trait.
    """
    _fd

  be _event_notify(event: Pointer[_Event] tag, flags: U32) =>
    """
    Handle socket events.
    """
    _Event.receive(event)

    if _Event.writeable(flags) then
      if event isnt _event then
        var fd = @os_socket_event_fd[U32](event)
        _connect_count = _connect_count - 1

        if not _connected then
          if @os_connected[Bool](fd) then
            _fd = fd
            _event = event
            _connected = true
            _notify.connected(this)
          else
            _Event.dispose(event)
            @os_closesocket[None](fd)

            if _connect_count == 0 then
              _notify.connect_failed(this)
              return
            end
          end
        else
          _Event.dispose(event)
          @os_closesocket[None](fd)
          return
        end
      end

      _writeable = true
    end

    if _Event.readable(flags) then
      _readable = true
    end

    _pending_writes()
    _pending_reads()

  be _read_again() =>
    """
    Resume reading.
    """
    _pending_reads()

  fun ref _pending_writes() =>
    """
    Send pending data. If any data can't be sent, keep it and mark as not
    writeable. On an error, dispose of the connection.
    """
    while _writeable and (_pending.size() > 0) do
      try
        var pending = _pending.head().item()
        var data = pending.data
        var offset = pending.offset

        var len = @os_send[U64](_fd, data.cstring().u64() + offset,
          data.size() - offset) ?

        if (len + offset) < data.size() then
          pending.offset = len + offset
          _writeable = false
        else
          _pending.pop()
        end
      else
        _close()
      end
    end

    if _closing and (_connect_count == 0) and (_pending.size() == 0) then
      _close()
    end

  fun ref _pending_reads() =>
    """
    Read while data is available, guessing the next packet length as we go. If
    we read 4 kb of data, send ourself a resume message and stop reading, to
    avoid starving other actors.
    """
    if _closing then
      return
    end

    try
      var sum: U64 = 0

      while _readable do
        var len = _last_read
        var data = recover Array[U8].undefined(len) end
        len = @os_recv[U64](_fd, data.cstring(), data.space()) ?

        match len
        | 0 =>
          _readable = false
          return
        | _last_read => _last_read = _last_read << 1
        end

        data.truncate(len)
        _notify.received(this, consume data)

        sum = sum + len

        if sum > (1 << 12) then
          _read_again()
          return
        end
      end
    else
      _close()
    end

  fun ref _notify_connecting() =>
    """
    Inform the notifier that we're connecting.
    """
    if _connect_count > 0 then
      _notify.connecting(this)
    else
      _notify.connect_failed(this)
    end

  fun ref _close() =>
    """
    Notify of disconnection and dispose of resoures.
    """
    if _connected then
      _notify.closed(this)
    end
    _final()

  fun ref _final() =>
    """
    Dispose of resources.
    """
    _event = _Event.dispose(_event)
    _connected = false
    _readable = false
    _writeable = false
    _closing = false

    if _fd != -1 then
      @os_closesocket[None](_fd)
      _fd = -1
    end

class _TCPPendingWrite
  """
  Unwritten data.
  """
  let data: Bytes val
  var offset: U64

  new create(data': Bytes val, offset': U64) =>
    data = data'
    offset = offset'
