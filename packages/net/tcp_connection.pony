use "collections"

use @pony_asio_event_create[AsioEventID](owner: AsioEventNotify, fd: U32,
  flags: U32, nsec: U64, noisy: Bool)
use @pony_asio_event_fd[U32](event: AsioEventID)
use @pony_asio_event_unsubscribe[None](event: AsioEventID)
use @pony_asio_event_resubscribe_read[None](event: AsioEventID)
use @pony_asio_event_resubscribe_write[None](event: AsioEventID)
use @pony_asio_event_destroy[None](event: AsioEventID)
use @pony_asio_event_set_writeable[None](event: AsioEventID, writeable: Bool)
use @pony_asio_event_set_readable[None](event: AsioEventID, readable: Bool)

type TCPConnectionAuth is (AmbientAuth | NetAuth | TCPAuth | TCPConnectAuth)

actor TCPConnection
  """
  A TCP connection. When connecting, the Happy Eyeballs algorithm is used.

  The following code creates a client that connects to port 8989 of
  the local host, writes "hello world", and listens for a response,
  which it then prints.

  ```pony
  use "net"

  class MyTCPConnectionNotify is TCPConnectionNotify
    let _out: OutStream

    new create(out: OutStream) =>
      _out = out

    fun ref connected(conn: TCPConnection ref) =>
      conn.write("hello world")

    fun ref received(
      conn: TCPConnection ref,
      data: Array[U8] iso,
      times: USize)
      : Bool
    =>
      _out.print("GOT:" + String.from_array(consume data))
      conn.close()
      true

    fun ref connect_failed(conn: TCPConnection ref) =>
      None

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

  use "collections"
  use "net"

  class SlowDown is TCPConnectionNotify
    let _coordinator: Coordinator

    new create(coordinator: Coordinator) =>
      _coordinator = coordinator

    fun ref throttled(connection: TCPConnection ref) =>
      _coordinator.throttled(connection)

    fun ref unthrottled(connection: TCPConnection ref) =>
      _coordinator.unthrottled(connection)

    fun ref connect_failed(conn: TCPConnection ref) =>
      None

  actor Coordinator
    var _senders: List[Sender] = _senders.create()

    be register(sender: Sender) =>
      _senders.push(sender)

    be throttled(connection: TCPConnection) =>
      for sender in _senders.values() do
        sender.pause_sending_to(connection)
      end

     be unthrottled(connection: TCPConnection) =>
      for sender in _senders.values() do
        sender.resume_sending_to(connection)
      end

  interface tag Sender
    be pause_sending_to(connection: TCPConnection)
    be resume_sending_to(connection: TCPConnection)

  actor Main
    new create(env: Env) =>
      let coordinator = Coordinator
      // Register senders in the coordinator here.
      try
        TCPConnection(env.root as AmbientAuth,
          recover SlowDown(coordinator) end, "", "8989")
      end
  ```

  Or if you want, you could handle backpressure by shedding load, that is,
  dropping the extra data rather than carrying out the send. This might look
  like:

  ```pony
  use "net"

  class ThrowItAway is TCPConnectionNotify
    var _throttled: Bool = false

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
        recover Array[String] end
      end

    fun ref throttled(connection: TCPConnection ref) =>
      _throttled = true

    fun ref unthrottled(connection: TCPConnection ref) =>
      _throttled = false

    fun ref connect_failed(conn: TCPConnection ref) =>
      None

  actor Main
    new create(env: Env) =>
      try
        TCPConnection(env.root as AmbientAuth,
          recover ThrowItAway end, "", "8989")
      end
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
  var _throttled: Bool = false
  var _closed: Bool = false
  var _shutdown: Bool = false
  var _shutdown_peer: Bool = false
  var _in_sent: Bool = false
  embed _pending: Array[ByteSeq] = _pending.create()
  embed _pending_writev: Array[USize] = _pending_writev.create()
  var _pending_sent: USize = 0
  var _pending_writev_total: USize = 0
  var _read_buf: Array[U8] iso

  var _next_size: USize
  let _max_size: USize

  var _read_len: USize = 0
  var _expect: USize = 0

  var _muted: Bool = false

  new create(
    auth: TCPConnectionAuth,
    notify: TCPConnectionNotify iso,
    host: String,
    service: String,
    from: String = "",
    init_size: USize = 64,
    max_size: USize = 16384)
  =>
    """
    Connect via IPv4 or IPv6. If `from` is a non-empty string, the connection
    will be made from the specified interface.
    """
    _read_buf = recover Array[U8] .> undefined(init_size) end
    _next_size = init_size
    _max_size = max_size
    _notify = consume notify
    _connect_count =
      @pony_os_connect_tcp[U32](this, host.cstring(), service.cstring(),
        from.cstring())
    _notify_connecting()

  new ip4(
    auth: TCPConnectionAuth,
    notify: TCPConnectionNotify iso,
    host: String,
    service: String,
    from: String = "",
    init_size: USize = 64,
    max_size: USize = 16384)
  =>
    """
    Connect via IPv4.
    """
    _read_buf = recover Array[U8] .> undefined(init_size) end
    _next_size = init_size
    _max_size = max_size
    _notify = consume notify
    _connect_count =
      @pony_os_connect_tcp4[U32](this, host.cstring(), service.cstring(),
        from.cstring())
    _notify_connecting()

  new ip6(
    auth: TCPConnectionAuth,
    notify: TCPConnectionNotify iso,
    host: String,
    service: String,
    from: String = "",
    init_size: USize = 64,
    max_size: USize = 16384)
  =>
    """
    Connect via IPv6.
    """
    _read_buf = recover Array[U8] .> undefined(init_size) end
    _next_size = init_size
    _max_size = max_size
    _notify = consume notify
    _connect_count =
      @pony_os_connect_tcp6[U32](this, host.cstring(), service.cstring(),
        from.cstring())
    _notify_connecting()

  new _accept(
    listen: TCPListener,
    notify: TCPConnectionNotify iso,
    fd: U32,
    init_size: USize = 64,
    max_size: USize = 16384)
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
    ifdef not windows then
      @pony_asio_event_set_writeable(_event, true)
    end
    _writeable = true
    _read_buf = recover Array[U8] .> undefined(init_size) end
    _next_size = init_size
    _max_size = max_size

    _notify.accepted(this)

    _readable = true
    _queue_read()
    _pending_reads()

  be write(data: ByteSeq) =>
    """
    Write a single sequence of bytes. Data will be silently discarded if the
    connection has not yet been established though.
    """
    if _connected and not _closed then
      _in_sent = true
      write_final(_notify.sent(this, data))
      _in_sent = false
    end

  be writev(data: ByteSeqIter) =>
    """
    Write a sequence of sequences of bytes. Data will be silently discarded if
    the connection has not yet been established though.
    """
    if _connected and not _closed then
      _in_sent = true

      ifdef windows then
        try
          var num_to_send: I32 = 0
          for bytes in _notify.sentv(this, data).values() do
            // Add an IOCP write.
            _pending_writev
              .> push(bytes.size())
              .> push(bytes.cpointer().usize())
            _pending_writev_total = _pending_writev_total + bytes.size()
            _pending.push(bytes)
            num_to_send = num_to_send + 1
          end

          // Write as much data as possible.
          var len =
            @pony_os_writev[USize](_event,
              _pending_writev.cpointer(_pending_sent * 2),
              num_to_send) ?

          _pending_sent = _pending_sent + num_to_send.usize()

          if _pending_sent > 32 then
            // If more than 32 asynchronous writes are scheduled, apply
            // backpressure. The choice of 32 is rather arbitrary an
            // probably needs tuning
            _apply_backpressure()
          end
        end
      else
        for bytes in _notify.sentv(this, data).values() do
          _pending_writev
            .> push(bytes.cpointer().usize())
            .> push(bytes.size())
          _pending_writev_total = _pending_writev_total + bytes.size()
          _pending.push(bytes)
        end

        _pending_writes()
      end

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
    Sets the TCP keepalive timeout to approximately `secs` seconds. Exact
    timing is OS dependent. If `secs` is zero, TCP keepalive is disabled. TCP
    keepalive is disabled by default. This can only be set on a connected
    socket.
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
            _readable = true

            _notify.connected(this)
            _queue_read()
            _pending_reads()

            // Don't call _complete_writes, as Windows will see this as a
            // closed connection.
            ifdef not windows then
              if _pending_writes() then
                // Sent all data; release backpressure.
                _release_backpressure()
              end
            end
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
        ifdef not windows then
          if _pending_writes() then
            // Sent all data. Release backpressure.
            _release_backpressure()
          end
        end
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
    Write as much as possible to the socket. Set `_writeable` to `false` if not
    everything was written. On an error, close the connection. This is for data
    that has already been transformed by the notifier. Data will be silently
    discarded if the connection has not yet been established though.
    """
    if _connected and not _closed then
      ifdef windows then
        try
          // Add an IOCP write.
          _pending_writev .> push(data.size()) .> push(data.cpointer().usize())
          _pending_writev_total = _pending_writev_total + data.size()
          _pending.push(data)

          @pony_os_writev[USize](_event,
            _pending_writev.cpointer(_pending_sent * 2), I32(1)) ?

          _pending_sent = _pending_sent + 1

          if _pending_sent > 32 then
            // If more than 32 asynchronous writes are scheduled, apply
            // backpressure. The choice of 32 is rather arbitrary an
            // probably needs tuning
            _apply_backpressure()
          end
        end
      else
        _pending_writev .> push(data.cpointer().usize()) .> push(data.size())
        _pending_writev_total = _pending_writev_total + data.size()
        _pending.push(data)
        _pending_writes()
      end
    end

  fun ref _complete_writes(len: U32) =>
    """
    The OS has informed us that `len` bytes of pending writes have completed.
    This occurs only with IOCP on Windows.
    """
    ifdef windows then
      if len == 0 then
        // IOCP reported a failed write on this chunk. Non-graceful shutdown.
        _hard_close()
        return
      end

      try
        _manage_pending_buffer(len.usize(),
          _pending_writev_total, _pending.size())?
      end

      if _pending_sent < 16 then
        // If fewer than 16 asynchronous writes are scheduled, remove
        // backpressure. The choice of 16 is rather arbitrary and probably
        // needs to be tuned.
        _release_backpressure()
      end
    end

  fun ref _pending_writes(): Bool =>
    """
    Send pending data. If any data can't be sent, keep it and mark as not
    writeable. On an error, dispose of the connection. Returns whether
    it sent all pending data or not.
    """
    ifdef not windows then
      // TODO: Make writev_batch_size user configurable
      let writev_batch_size: USize = @pony_os_writev_max[I32]().usize()
      var num_to_send: USize = 0
      var bytes_to_send: USize = 0
      while _writeable and (_pending_writev_total > 0) do
        try
          // Determine number of bytes and buffers to send.
          if (_pending_writev.size() / 2) < writev_batch_size then
            num_to_send = _pending_writev.size() / 2
            bytes_to_send = _pending_writev_total
          else
            // Have more buffers than a single writev can handle.
            // Iterate over buffers being sent to add up total.
            num_to_send = writev_batch_size
            bytes_to_send = 0
            for d in Range[USize](1, num_to_send * 2, 2) do
              bytes_to_send = bytes_to_send + _pending_writev(d)?
            end
          end

          // Write as much data as possible.
          var len = @pony_os_writev[USize](_event,
            _pending_writev.cpointer(), num_to_send.i32()) ?

          if _manage_pending_buffer(len, bytes_to_send, num_to_send)? then
            return true
          end
        else
          // Non-graceful shutdown on error.
          _hard_close()
        end
      end
    end

    false

  fun ref _manage_pending_buffer(
    bytes_sent: USize,
    bytes_to_send: USize,
    num_to_send: USize)
    : Bool ?
  =>
    """
    Manage pending buffer for data sent. Returns a boolean of whether
    the pending buffer is empty or not.
    """
    var len = bytes_sent
    if len < bytes_to_send then
      while len > 0 do
        let iov_p =
          ifdef windows then
            _pending_writev(1)?
          else
            _pending_writev(0)?
          end
        let iov_s =
          ifdef windows then
            _pending_writev(0)?
          else
            _pending_writev(1)?
          end
        if iov_s <= len then
          len = len - iov_s
          _pending_writev.shift()?
          _pending_writev.shift()?
          _pending.shift()?
          ifdef windows then
            _pending_sent = _pending_sent - 1
          end
          _pending_writev_total = _pending_writev_total - iov_s
        else
          ifdef windows then
            _pending_writev.update(1, iov_p+len)?
            _pending_writev.update(0, iov_s-len)?
          else
            _pending_writev.update(0, iov_p+len)?
            _pending_writev.update(1, iov_s-len)?
          end
          _pending_writev_total = _pending_writev_total - len
          len = 0
        end
      end
      ifdef not windows then
        _apply_backpressure()
      end
    else
      // sent all data we requested in this batch
      _pending_writev_total = _pending_writev_total - bytes_to_send
      if _pending_writev_total == 0 then
        _pending_writev.clear()
        _pending.clear()
        ifdef windows then
          _pending_sent = 0
        end
        return true
      else
        for d in Range[USize](0, num_to_send, 1) do
          _pending_writev.shift()?
          _pending_writev.shift()?
          _pending.shift()?
          ifdef windows then
            _pending_sent = _pending_sent - 1
          end
        end
      end
    end

    false

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

        _notify.received(this, consume data, 1)
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
          _read_buf.cpointer(_read_len),
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
        var received_called: USize = 0

        while _readable and not _shutdown_peer do
          if _muted then
            return
          end

          // Read as much data as possible.
          let len = @pony_os_recv[USize](
            _event,
            _read_buf.cpointer(_read_len),
            _read_buf.size() - _read_len) ?

          match len
          | 0 =>
            // Would block, try again later.
            // this is safe because asio thread isn't currently subscribed
            // for a read event so will not be writing to the readable flag
            @pony_asio_event_set_readable(_event, false)
            _readable = false
            @pony_asio_event_resubscribe_read(_event)
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

            received_called = received_called + 1
            if not _notify.received(this, consume data, received_called) then
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
    length read. If the connection is muted, perform a hard close and shut
    down immediately.
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
      (_pending_writev_total == 0)
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
      if not _connected and not _readable and (_pending_sent == 0) then
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

    _pending.clear()
    _pending_writev.clear()
    _pending_writev_total = 0
    ifdef windows then
      _pending_sent = 0
    end

    ifdef not windows then
      // Unsubscribe immediately and drop all pending writes.
      @pony_asio_event_unsubscribe(_event)
      _readable = false
      _writeable = false
      @pony_asio_event_set_readable(_event, false)
      @pony_asio_event_set_writeable(_event, false)
    end

    // On windows, this will also cancel all outstanding IOCP operations.
    @pony_os_socket_close[None](_fd)
    _fd = -1

    _notify.closed(this)

    try (_listen as TCPListener)._conn_closed() end

  fun ref _apply_backpressure() =>
    if not _throttled then
      _throttled = true
      ifdef not windows then
        _writeable = false

        // this is safe because asio thread isn't currently subscribed
        // for a write event so will not be writing to the readable flag
        @pony_asio_event_set_writeable(_event, false)
        @pony_asio_event_resubscribe_write(_event)
      end

      _notify.throttled(this)
    end

  fun ref _release_backpressure() =>
    if _throttled then
      _throttled = false
      _notify.unthrottled(this)
    end
