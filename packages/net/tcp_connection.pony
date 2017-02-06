use "collections"

use @pony_asio_event_create[AsioEventID](owner: AsioEventNotify, fd: U32,
  flags: U32, nsec: U64, noisy: Bool)
use @pony_asio_event_fd[U32](event: AsioEventID)
use @pony_asio_event_unsubscribe[None](event: AsioEventID)
use @pony_asio_event_resubscribe[None](event: AsioEventID, flags: U32)
use @pony_asio_event_destroy[None](event: AsioEventID)

type TCPConnectionAuth is (AmbientAuth | NetAuth | TCPAuth | TCPConnectAuth)

actor TCPConnection
  """
  A TCP connection. When connecting, the Happy Eyeballs algorithm is used.

  The following code creates a client that connects to port 8989 of
  the local host, writes "hello world", and listens for a response,
  which it then prints.

  ```
  use "net"

  class MyTCPConnectionNotify is TCPConnectionNotify
    let _out: OutStream

    new create(out: OutStream) =>
      _out = out

    fun ref connected(conn: TCPConnection ref) =>
      conn.write("hello world")

    fun ref received(conn: TCPConnection ref, data: Array[U8] iso) =>
      _out.print("GOT:" + String.from_array(consume data))
      conn.close()

  actor Main
    new create(env: Env) =>
      try
        TCPConnection(env.root as AmbientAuth,
          recover MyTCPConnectionNotify(env.out) end, "", "8989")
      end
  ```

  Note: when writing to the connection data will be silently discarded if the
  connection has not yet been established.

  ## Backpressure support

  ### Write

  The TCP protocol has built-in backpressure support. This is generally
  experienced as the outgoing write buffer becoming full and being unable
  to write all requested data to the socket. In `TCPConnection`, this is
  hidden from the programmer. When this occurs, `TCPConnection` will buffer
  the extra data until such time as it is able to be sent. Left unchecked,
  this could result in uncontrolled queuing. To address this,
  `TCPConnectionNotify` implements two methods `throttled` and `unthrottled`
  that are called when backpressure is applied and released.

  Upon receiving a `throttled` notification, your application has two choices
  on how to handle it. One is to inform any actors sending the connection to
  stop sending data. For example, you might construct your application like:

  ```pony
  // Here we have a TCPConnectionNotify that upon construction
  // is given a tag to a "Coordinator". Any actors that want to
  // "publish" to this connection should register with the
  // coordinator. This allows the notifier to inform the coordinator
  // that backpressure has been applied and then it in turn can
  // notify senders who could then pause sending.

  class SlowDown is TCPConnectionNotify
    let _coordinator: Coordinator

    new create(coordinator: Coordinator) =>
      _coordinator = coordinator

    fun ref throttled(connection: TCPConnection ref) =>
      _coordinator.throttled(this)

    fun ref unthrottled(connection: TCPConnection ref) =>
      _coordinator.unthrottled(this)

  actor Coordinator
    var _senders: List[Any tag] = _senders.create()

    be register(sender: Any tag) =>
      _senders.push(sender)

    be throttled(connection: TCPConnection) =>
      for sender in _senders.values() do
        sender.pause_sending_to(connection)
      end

     be unthrottled(connection: TCPConnection) =>
      for sender in _senders.values() do
        sender.resume_sending_to(connection)
      end
  ```

  Or if you want, you could handle backpressure by shedding load, that is,
  dropping the extra data rather than carrying out the send. This might look
  like:

  ```pony
  class ThrowItAway is TCPConnectionNotify
    var _throttled = false

    fun ref sent(conn: TCPConnection ref, data: ByteSeq): ByteSeq =>
      if not _throttled then
        data
      else
        ""
      end

  fun ref sentv(conn: TCPConnection ref, data: ByteSeqIter): ByteSeqIter =>
    if not _throttled then
      data
    else
      Array[String]
    end

    fun ref throttled(connection: TCPConnection ref) =>
      _throttled = true

    fun ref unthrottled(connection: TCPConnection ref) =>
      _throttled = false
  ```

  In general, unless you have a very specific use case, we strongly advise that
  you don't implement a load shedding scheme where you drop data.

  ### Read

  If your application is unable to keep up with data being sent to it over
  a `TCPConnection` you can use the builtin read backpressure support to
  pause reading the socket which will in turn start to exert backpressure on
  the corresponding writer on the other end of that socket.

  The `mute` behavior allow any other actors in your application to request
  the cessation of additional reads until such time as `unmute` is called.
  Please note that this cessation is not guaranteed to happen immediately as
  it is the result of an asynchronous behavior call and as such will have to
  wait for existing messages in the `TCPConnection`'s mailbox to be handled.

  On non-windows platforms, your `TCPConnection` will not notice if the
  other end of the connection closes until you unmute it. Unix type systems
  like FreeBSD, Linux and OSX learn about a closed connection upon read. On
  these platforms, you **must** call `unmute` on a muted connection to have
  it close. Without calling `unmute` the `TCPConnection` actor will never
  exit.
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

  var _muted: Bool = false

  new create(auth: TCPConnectionAuth, notify: TCPConnectionNotify iso,
    host: String, service: String, from: String = "", init_size: USize = 64,
    max_size: USize = 16384)
  =>
    """
    Connect via IPv4 or IPv6. If `from` is a non-empty string, the connection
    will be made from the specified interface.
    """
    _read_buf = recover Array[U8].>undefined(init_size) end
    _next_size = init_size
    _max_size = max_size
    _notify = consume notify
    _connect_count = @pony_os_connect_tcp[U32](this,
      host.cstring(), service.cstring(),
      from.cstring())
    _notify_connecting()

  new ip4(auth: TCPConnectionAuth, notify: TCPConnectionNotify iso,
    host: String, service: String, from: String = "", init_size: USize = 64,
    max_size: USize = 16384)
  =>
    """
    Connect via IPv4.
    """
    _read_buf = recover Array[U8].>undefined(init_size) end
    _next_size = init_size
    _max_size = max_size
    _notify = consume notify
    _connect_count = @pony_os_connect_tcp4[U32](this,
      host.cstring(), service.cstring(),
      from.cstring())
    _notify_connecting()

  new ip6(auth: TCPConnectionAuth, notify: TCPConnectionNotify iso,
    host: String, service: String, from: String = "", init_size: USize = 64,
    max_size: USize = 16384)
  =>
    """
    Connect via IPv6.
    """
    _read_buf = recover Array[U8].>undefined(init_size) end
    _next_size = init_size
    _max_size = max_size
    _notify = consume notify
    _connect_count = @pony_os_connect_tcp6[U32](this,
      host.cstring(), service.cstring(),
      from.cstring())
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
    ifdef not windows then
      _event = @pony_asio_event_create(this, fd,
        AsioEvent.read_write_oneshot(), 0, true)
    else
      _event = @pony_asio_event_create(this, fd,
        AsioEvent.read_write(), 0, true)
    end
    _connected = true
    _writeable = true
    _read_buf = recover Array[U8].>undefined(init_size) end
    _next_size = init_size
    _max_size = max_size

    _notify.accepted(this)
    _queue_read()

  be write(data: ByteSeq) =>
    """
    Write a single sequence of bytes. Data will be silently discarded if the
    connection has not yet been established though.
    """
    if _connected and not _closed then
      _in_sent = true
      write_final(_notify.sent(this, data))
      _resubscribe_event()
      _in_sent = false
    end

  be writev(data: ByteSeqIter) =>
    """
    Write a sequence of sequences of bytes. Data will be silently discarded if
    the connection has not yet been established though.
    """
    if _connected and not _closed then
      _in_sent = true

      for bytes in _notify.sentv(this, data).values() do
        write_final(bytes)
      end
      _resubscribe_event()

      _in_sent = false
    end

  be mute() =>
    """
    Temporarily suspend reading off this TCPConnection until such time as
    `unmute` is called.
    """
    _muted = true

  be unmute() =>
    """
    Start reading off this TCPConnection again after having been muted.
    """
    _muted = false
    _pending_reads()

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

  fun local_address(): NetAddress =>
    """
    Return the local IP address.
    """
    let ip = recover NetAddress end
    @pony_os_sockname[Bool](_fd, ip)
    ip

  fun remote_address(): NetAddress =>
    """
    Return the remote IP address.
    """
    let ip = recover NetAddress end
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
    Sets the TCP keepalive timeout to approximately `secs` seconds. Exact timing
    is OS dependent. If `secs` is zero, TCP keepalive is disabled. TCP keepalive
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
    _resubscribe_event()

  be _read_again() =>
    """
    Resume reading.
    """
    _pending_reads()
    _resubscribe_event()

  fun ref write_final(data: ByteSeq) =>
    """
    Write as much as possible to the socket. Set `_writeable` to `false` if not
    everything was written. On an error, close the connection. This is for data
    that has already been transformed by the notifier. Data will be silently
    discarded if the connection has not yet been established though.
    """
    if _connected and not _closed then
      ifdef windows then
        try
          // Add an IOCP write.
          @pony_os_send[USize](_event, data.cpointer(), data.size()) ?
          _pending.push((data, 0))

          if _pending.size() > 32 then
            // If more than 32 asynchronous writes are scheduled, apply
            // backpressure. The choice of 32 is rather arbitrary an
            // probably needs tuning
            _apply_backpressure()
          end
        end
      else
        if _writeable then
          try
            // Send as much data as possible.
            var len =
              @pony_os_send[USize](_event, data.cpointer(), data.size()) ?

            if len < data.size() then
              // Send any remaining data later.
              _pending.push((data, len))
              _apply_backpressure()
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
    The OS has informed us that `len` bytes of pending writes have completed.
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

      if _pending.size() < 16 then
        // If fewer than 16 asynchronous writes are scheduled, remove
        // backpressure. The choice of 16 is rather arbitrary and probably
        // needs to be tuned.
        _release_backpressure()
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
            data.cpointer().usize() + offset, data.size() - offset) ?

          if (len + offset) < data.size() then
            // Send remaining data later.
            node() = (data, offset + len)
            _apply_backpressure()
          else
            // This chunk has been fully sent.
            _pending.shift()

            if _pending.size() == 0 then
              _release_backpressure()
            end
          end
        else
          // Non-graceful shutdown on error.
          _hard_close()
        end
      end
    end

  fun ref _complete_reads(len: U32) =>
    """
    The OS has informed us that `len` bytes of pending reads have completed.
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

      if (not _muted) and (_read_len >= _expect) then
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
          _read_buf.cpointer().usize() + _read_len,
          _read_buf.size() - _read_len) ?
      else
        _hard_close()
      end
    end

  fun ref _pending_reads() =>
    """
    Unless this connection is currently muted, read while data is available,
    guessing the next packet length as we go. If we read 4 kb of data, send
    ourself a resume message and stop reading, to avoid starving other actors.
    """
    ifdef not windows then
      try
        var sum: USize = 0

        while _readable and not _shutdown_peer do
          if _muted then
            return
          end

          // Read as much data as possible.
          let len = @pony_os_recv[USize](
            _event,
            _read_buf.cpointer().usize() + _read_len,
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

            if not _notify.received(this, consume data) then
              _read_buf_size()
              _read_again()
              return
            else
              _read_buf_size()
            end
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
    Attempt to perform a graceful shutdown. Don't accept new writes. If the
    connection isn't muted then we won't finish closing until we get a zero
    length read.  If the connection is muted, perform a hard close and
    shut down immediately.
    """
     ifdef windows then
      _close()
    else
      if _muted then
        _hard_close()
      else
       _close()
     end
    end

  fun ref _close() =>
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

  fun ref _apply_backpressure() =>
    ifdef not windows then
      _writeable = false
    end

    _notify.throttled(this)

  fun ref _release_backpressure() =>
    _notify.unthrottled(this)

  fun ref _resubscribe_event() =>
    ifdef not windows then
      let flags = if not _readable and not _writeable then
        AsioEvent.read_write_oneshot()
      elseif not _readable then
        AsioEvent.read() or AsioEvent.oneshot()
      elseif not _writeable then
        AsioEvent.write() or AsioEvent.oneshot()
      else
        return
      end

      @pony_asio_event_resubscribe(_event, flags)
    end
