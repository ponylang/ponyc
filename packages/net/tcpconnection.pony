use "collections"

use @pony_asio_event_create[AsioEventID](owner: AsioEventNotify, fd: U32,
  flags: U32, nsec: U64, noisy: Bool)
use @pony_asio_event_fd[U32](event: AsioEventID)
use @pony_asio_event_unsubscribe[None](event: AsioEventID)
use @pony_asio_event_destroy[None](event: AsioEventID)

type TCPConnectionAuth is (AmbientAuth | NetAuth | TCPAuth | TCPConnectAuth)

actor TCPConnection
  """
  A TCP connection. When connecting, the Happy Eyeballs algorithm is used.
  """
  var _listen: (TCPListener | None) = None
  var _notify: TCPConnectionNotify
  var _connect_count: U32
  var _fd: U32 = -1
  var _event: AsioEventID = AsioEvent.none()
  var _connected: Bool = false
  var _readable: Bool = false
  var _writeable: Bool = false
  var _closed: Bool = false
  var _shutdown: Bool = false
  var _shutdown_peer: Bool = false
  var _in_sent: Bool = false
  embed _pending: List[(ByteSeq, USize)] = _pending.create()
  var _read_buf: Array[U8] iso

  var _next_size: USize
  let _max_size: USize

  var _read_len: USize = 0
  var _expect: USize = 0

  new create(auth: TCPConnectionAuth, notify: TCPConnectionNotify iso,
    host: String, service: String, from: String = "", init_size: USize = 64,
    max_size: USize = 16384)
  =>
    """
    Connect via IPv4 or IPv6. If `from` is a non-empty string, the connection
    will be made from the specified interface.
    """
    _read_buf = recover Array[U8].undefined(init_size) end
    _next_size = init_size
    _max_size = max_size
    _notify = consume notify
    _connect_count = @pony_os_connect_tcp[U32](this, host.cstring(),
      service.cstring(), from.cstring())
    _notify_connecting()

  new ip4(auth: TCPConnectionAuth, notify: TCPConnectionNotify iso,
    host: String, service: String, from: String = "", init_size: USize = 64,
    max_size: USize = 16384)
  =>
    """
    Connect via IPv4.
    """
    _read_buf = recover Array[U8].undefined(init_size) end
    _next_size = init_size
    _max_size = max_size
    _notify = consume notify
    _connect_count = @pony_os_connect_tcp4[U32](this, host.cstring(),
      service.cstring(), from.cstring())
    _notify_connecting()

  new ip6(auth: TCPConnectionAuth, notify: TCPConnectionNotify iso,
    host: String, service: String, from: String = "", init_size: USize = 64,
    max_size: USize = 16384)
  =>
    """
    Connect via IPv6.
    """
    _read_buf = recover Array[U8].undefined(init_size) end
    _next_size = init_size
    _max_size = max_size
    _notify = consume notify
    _connect_count = @pony_os_connect_tcp6[U32](this, host.cstring(),
      service.cstring(), from.cstring())
    _notify_connecting()

  new _accept(listen: TCPListener, notify: TCPConnectionNotify iso, fd: U32,
    init_size: USize = 64, max_size: USize = 16384)
  =>
    """
    A new connection accepted on a server.
    """
    _listen = listen
    _notify = consume notify
    _connect_count = 0
    _fd = fd
    _event = @pony_asio_event_create(this, fd, AsioEvent.read_write(), 0, true)
    _connected = true
    _writeable = true
    _read_buf = recover Array[U8].undefined(init_size) end
    _next_size = init_size
    _max_size = max_size

    _notify.accepted(this)
    _queue_read()

  be write(data: ByteSeq) =>
    """
    Write a single sequence of bytes.
    """
    if not _closed then
      _in_sent = true
      try write_final(_notify.sent(this, data)) end
      _in_sent = false
    end

  be writev(data: ByteSeqIter) =>
    """
    Write a sequence of sequences of bytes.
    """
    if not _closed then
      _in_sent = true
      try
        for bytes in _notify.sentv(this, data).values() do
          write_final(bytes)
        end
      else
        for bytes in data.values() do
          try write_final(_notify.sent(this, bytes)) end
        end
      end
      _in_sent = false
    end

  be set_notify(notify: TCPConnectionNotify iso) =>
    """
    Change the notifier.
    """
    _notify = consume notify

  be dispose() =>
    """
    Close the connection gracefully once all writes are sent.
    """
    close()

  fun local_address(): IPAddress =>
    """
    Return the local IP address.
    """
    let ip = recover IPAddress end
    @pony_os_sockname[Bool](_fd, ip)
    ip

  fun remote_address(): IPAddress =>
    """
    Return the remote IP address.
    """
    let ip = recover IPAddress end
    @pony_os_peername[Bool](_fd, ip)
    ip

  fun ref expect(qty: USize = 0) =>
    """
    A `received` call on the notifier must contain exactly `qty` bytes. If
    `qty` is zero, the call can contain any amount of data. This has no effect
    if called in the `sent` notifier callback.
    """
    if not _in_sent then
      _expect = _notify.expect(this, qty)
      _read_buf_size()
    end

  fun ref set_nodelay(state: Bool) =>
    """
    Turn Nagle on/off. Defaults to on. This can only be set on a connected
    socket.
    """
    if _connected then
      @pony_os_nodelay[None](_fd, state)
    end

  fun ref set_keepalive(secs: U32) =>
    """
    Sets the TCP keepalive timeout to approximately secs seconds. Exact timing
    is OS dependent. If secs is zero, TCP keepalive is disabled. TCP keepalive
    is disabled by default. This can only be set on a connected socket.
    """
    if _connected then
      @pony_os_keepalive[None](_fd, secs)
    end

  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    """
    Handle socket events.
    """
    if event isnt _event then
      if AsioEvent.writeable(flags) then
        // A connection has completed.
        var fd = @pony_asio_event_fd(event)
        _connect_count = _connect_count - 1

        if not _connected and not _closed then
          // We don't have a connection yet.
          if @pony_os_connected[Bool](fd) then
            // The connection was successful, make it ours.
            _fd = fd
            _event = event
            _connected = true
            _writeable = true

            _notify.connected(this)
            _queue_read()

            // Don't call _complete_writes, as Windows will see this as a
            // closed connection.
            _pending_writes()
          else
            // The connection failed, unsubscribe the event and close.
            @pony_asio_event_unsubscribe(event)
            @pony_os_socket_close[None](fd)
            _notify_connecting()
          end
        else
          // We're already connected, unsubscribe the event and close.
          @pony_asio_event_unsubscribe(event)
          @pony_os_socket_close[None](fd)
        end
      else
        // It's not our event.
        if AsioEvent.disposable(flags) then
          // It's disposable, so dispose of it.
          @pony_asio_event_destroy(event)
        end
      end
    else
      // At this point, it's our event.
      if AsioEvent.writeable(flags) then
        _writeable = true
        _complete_writes(arg)
        _pending_writes()
      end

      if AsioEvent.readable(flags) then
        _readable = true
        _complete_reads(arg)
        _pending_reads()
      end

      if AsioEvent.disposable(flags) then
        @pony_asio_event_destroy(event)
        _event = AsioEvent.none()
      end

      _try_shutdown()
    end

  be _read_again() =>
    """
    Resume reading.
    """
    _pending_reads()

  fun ref write_final(data: ByteSeq) =>
    """
    Write as much as possible to the socket. Set _writeable to false if not
    everything was written. On an error, close the connection. This is for
    data that has already been transformed by the notifier.
    """
    if not _closed then
      ifdef windows then
        try
          // Add an IOCP write.
          @pony_os_send[USize](_event, data.cstring(), data.size()) ?
          _pending.push((data, 0))
        end
      else
        if _writeable then
          try
            // Send as much data as possible.
            var len =
              @pony_os_send[USize](_event, data.cstring(), data.size()) ?

            if len < data.size() then
              // Send any remaining data later.
              _pending.push((data, len))
              _writeable = false
            end
          else
            // Non-graceful shutdown on error.
            _hard_close()
          end
        else
          // Send later, when the socket is available for writing.
          _pending.push((data, 0))
        end
      end
    end

  fun ref _complete_writes(len: U32) =>
    """
    The OS has informed as that len bytes of pending writes have completed.
    This occurs only with IOCP on Windows.
    """
    ifdef windows then
      var rem = len.usize()

      if rem == 0 then
        // IOCP reported a failed write on this chunk. Non-graceful shutdown.
        try _pending.shift() end
        _hard_close()
        return
      end

      while rem > 0 do
        try
          let node = _pending.head()
          (let data, let offset) = node()
          let total = rem + offset

          if total < data.size() then
            node() = (data, total)
            rem = 0
          else
            _pending.shift()
            rem = total - data.size()
          end
        end
      end
    end

  fun ref _pending_writes() =>
    """
    Send pending data. If any data can't be sent, keep it and mark as not
    writeable. On an error, dispose of the connection.
    """
    ifdef not windows then
      while _writeable and (_pending.size() > 0) do
        try
          let node = _pending.head()
          (let data, let offset) = node()

          // Write as much data as possible.
          let len = @pony_os_send[USize](_event,
            data.cstring().usize() + offset, data.size() - offset) ?

          if (len + offset) < data.size() then
            // Send remaining data later.
            node() = (data, offset + len)
            _writeable = false
          else
            // This chunk has been fully sent.
            _pending.shift()
          end
        else
          // Non-graceful shutdown on error.
          _hard_close()
        end
      end
    end

  fun ref _complete_reads(len: U32) =>
    """
    The OS has informed as that len bytes of pending reads have completed.
    This occurs only with IOCP on Windows.
    """
    ifdef windows then
      match len.usize()
      | 0 =>
        // The socket has been closed from the other side, or a hard close has
        // cancelled the queued read.
        _readable = false
        _shutdown_peer = true
        close()
        return
      | _next_size =>
        _next_size = _max_size.min(_next_size * 2)
      end

      _read_len = _read_len + len.usize()

      if _read_len >= _expect then
        let data = _read_buf = recover Array[U8] end
        data.truncate(_read_len)
        _read_len = 0

        _notify.received(this, consume data)
        _read_buf_size()
      end

      _queue_read()
    end

  fun ref _read_buf_size() =>
    """
    Resize the read buffer.
    """
    if _expect != 0 then
      _read_buf.undefined(_expect)
    else
      _read_buf.undefined(_next_size)
    end

  fun ref _queue_read() =>
    """
    Begin an IOCP read on Windows.
    """
    ifdef windows then
      try
        @pony_os_recv[USize](
          _event,
          _read_buf.cstring().usize() + _read_len,
          _read_buf.size() - _read_len) ?
      else
        _hard_close()
      end
    end

  fun ref _pending_reads() =>
    """
    Read while data is available, guessing the next packet length as we go. If
    we read 4 kb of data, send ourself a resume message and stop reading, to
    avoid starving other actors.
    """
    ifdef not windows then
      try
        var sum: USize = 0

        while _readable and not _shutdown_peer do
          // Read as much data as possible.
          let len = @pony_os_recv[USize](
            _event,
            _read_buf.cstring().usize() + _read_len,
            _read_buf.size() - _read_len) ?

          match len
          | 0 =>
            // Would block, try again later.
            _readable = false
            return
          | _next_size =>
            // Increase the read buffer size.
            _next_size = _max_size.min(_next_size * 2)
          end

          _read_len = _read_len + len

          if _read_len >= _expect then
            let data = _read_buf = recover Array[U8] end
            data.truncate(_read_len)
            _read_len = 0

            _notify.received(this, consume data)
            _read_buf_size()
          end

          sum = sum + len

          if sum >= _max_size then
            // If we've read _max_size, yield and read again later.
            _read_again()
            return
          end
        end
      else
        // The socket has been closed from the other side.
        _shutdown_peer = true
        close()
      end
    end

  fun ref _notify_connecting() =>
    """
    Inform the notifier that we're connecting.
    """
    if _connect_count > 0 then
      _notify.connecting(this, _connect_count)
    else
      _notify.connect_failed(this)
      _hard_close()
    end

  fun ref close() =>
    """
    Perform a graceful shutdown. Don't accept new writes, but don't finish
    closing until we get a zero length read.
    """
    _closed = true
    _try_shutdown()

  fun ref _try_shutdown() =>
    """
    If we have closed and we have no remaining writes or pending connections,
    then shutdown.
    """
    if not _closed then
      return
    end

    if
      not _shutdown and
      (_connect_count == 0) and
      (_pending.size() == 0)
    then
      _shutdown = true

      if _connected then
        @pony_os_socket_shutdown[None](_fd)
      else
        _shutdown_peer = true
      end
    end

    if _connected and _shutdown and _shutdown_peer then
      _hard_close()
    end

    ifdef windows then
      // On windows, wait until all outstanding IOCP operations have completed
      // or been cancelled.
      if not _connected and not _readable and (_pending.size() == 0) then
        @pony_asio_event_unsubscribe(_event)
      end
    end

  fun ref _hard_close() =>
    """
    When an error happens, do a non-graceful close.
    """
    if not _connected then
      return
    end

    _connected = false
    _closed = true
    _shutdown = true
    _shutdown_peer = true

    ifdef not windows then
      // Unsubscribe immediately and drop all pending writes.
      @pony_asio_event_unsubscribe(_event)
      _pending.clear()
      _readable = false
      _writeable = false
    end

    // On windows, this will also cancel all outstanding IOCP operations.
    @pony_os_socket_close[None](_fd)
    _fd = -1

    _notify.closed(this)

    try (_listen as TCPListener)._conn_closed() end
