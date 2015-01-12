use "collections"
use "events"

actor TCPConnection
  """
  A TCP connection. When connecting, the Happy Eyeballs algorithm is used.
  """
  var _notify: TCPConnectionNotify
  var _connect_count: U32
  var _fd: U32 = -1
  var _event: Pointer[Event] tag = Pointer[Event]
  var _connected: Bool = false
  var _readable: Bool = false
  var _writeable: Bool = false
  var _closed: Bool = false
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
    _event = Event.socket(this, fd)
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
          _pending.push(_TCPPendingWrite(data, len))
          _writeable = false
        end
      else
        _close()
      end
    elseif not _closed then
      _pending.push(_TCPPendingWrite(data, 0))
    end

  be dispose() =>
    """
    Close the connection once all writes are sent.
    """
    _closed = true

    if (_connect_count == 0) and (_pending.size() == 0) then
      _close()
    end

  fun local_address(): IPAddress =>
    """
    Return the local IP address.
    """
    let ip = recover IPAddress end
    @os_sockname[None](_fd, ip)
    ip

  fun remote_address(): IPAddress =>
    """
    Return the remote IP address.
    """
    let ip = recover IPAddress end
    @os_peername[None](_fd, ip)
    ip

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

  be _event_notify(event: Pointer[Event] tag, flags: U32) =>
    """
    Handle socket events.
    """
    if event isnt _event then
      if Event.writeable(flags) then
        // A connection has completed.
        var fd = Event.fd(event)
        _connect_count = _connect_count - 1

        if not _connected and not _closed then
          // We don't have a connection yet.
          if @os_connected[Bool](fd) then
            // The connection was successful, make it ours.
            _fd = fd
            _event = event
            _connected = true
            _notify.connected(this)
          else
            // The connection failed, unsubscribe the event and close.
            Event.unsubscribe(event)
            @os_closesocket[None](fd)

            if _connect_count == 0 then
              // All connections failed.
              _notify.connect_failed(this)
            end

            // We're done.
            return
          end
        else
          // We're already connected, unsubscribe the event and close.
          Event.unsubscribe(event)
          @os_closesocket[None](fd)

          // We're done.
          return
        end
      else
        // It's not our event.
        if Event.disposable(flags) then
          // It's disposable, so dispose of it.
          Event.dispose(event)
        end

        // We're done.
        return
      end
    end

    // At this point, it's our event.
    if not _closed then
      if Event.writeable(flags) then
        _writeable = true
        _pending_writes()
      end

      if Event.readable(flags) then
        _readable = true
        _pending_reads()
      end
    end

    if Event.disposable(flags) then
      _event = Event.dispose(_event)
    end

  be _read_again() =>
    """
    Resume reading.
    """
    if not _closed then
      _pending_reads()
    end

  fun ref _pending_writes() =>
    """
    Send pending data. If any data can't be sent, keep it and mark as not
    writeable. On an error, dispose of the connection.
    """
    while _writeable and (_pending.size() > 0) do
      try
        var pending = _pending.head()()
        var data = pending.data
        var offset = pending.offset

        var len = @os_send[U64](_fd, data.cstring().u64() + offset,
          data.size() - offset) ?

        if (len + offset) < data.size() then
          pending.offset = len + offset
          _writeable = false
        else
          _pending.shift()
        end
      else
        _close()
      end
    end

    if _closed and (_connect_count == 0) and (_pending.size() == 0) then
      _close()
    end

  fun ref _pending_reads() =>
    """
    Read while data is available, guessing the next packet length as we go. If
    we read 4 kb of data, send ourself a resume message and stop reading, to
    avoid starving other actors.
    """
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

    Event.unsubscribe(_event)
    _connected = false
    _readable = false
    _writeable = false
    _closed = true

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
