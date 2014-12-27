use "collections"

actor TCPConnection is Socket
  """
  A TCP connection.
  """
  var _notify: TCPConnectionNotify
  var _fd: U32
  var _event: Pointer[_Event]
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
    _fd = @os_connect_tcp[U32](host.cstring(), service.cstring())
    _event = _Event.socket(this, _fd)

  new ip4(notify: TCPConnectionNotify iso, host: String, service: String) =>
    """
    Connect via IPv4.
    """
    _notify = consume notify
    _fd = @os_connect_tcp4[U32](host.cstring(), service.cstring())
    _event = _Event.socket(this, _fd)

  new ip6(notify: TCPConnectionNotify iso, host: String, service: String) =>
    """
    Connect via IPv6.
    """
    _notify = consume notify
    _fd = @os_connect_tcp6[U32](host.cstring(), service.cstring())
    _event = _Event.socket(this, _fd)

  new _accept(notify: TCPConnectionNotify iso, fd: U32) =>
    """
    A new connection accepted on a server.
    """
    _notify = consume notify
    _fd = fd
    _event = _Event.socket(this, _fd)
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
    elseif _connected and not _closing then
      _pending.append(_TCPPendingWrite(data, 0))
    end

  be dispose() =>
    """
    Close the connection once all writes are sent.
    """
    _closing = true

    if _pending.size() == 0 then
      _close()
    end

  fun ref set_notify(notify: TCPConnectionNotify ref) =>
    """
    Change the notifier.
    """
    _notify = notify

  fun box _get_fd(): U32 =>
    _fd

  be _event_notify(event: Pointer[_Event] tag, flags: U32) =>
    """
    Handle socket events.
    """
    if _Event.writeable(flags) then
      if not _connected then
        if not @os_connected[Bool](_fd) then
          // TODO: what if we got multiple addresses from getaddrinfo?
          _notify.connect_failed(this)
          return _close()
        end

        _notify.connected(this)
        _connected = true
      end

      _writeable = true
    end

    if _Event.readable(flags) then
      _readable = true
    end

    // TODO: error events?

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

    if _closing and (_pending.size() == 0) then
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
        _notify.read(this, consume data)

        sum = sum + len

        if sum > (1 << 12) then
          _read_again()
          return
        end
      end
    else
      _close()
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
