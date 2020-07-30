use "collections"

use @pony_asio_event_create[AsioEventID](owner: AsioEventNotify, fd: U32,
  flags: U32, nsec: U64, noisy: Bool)
use @pony_asio_event_fd[U32](event: AsioEventID)
use @pony_asio_event_unsubscribe[None](event: AsioEventID)
use @pony_asio_event_resubscribe_read[None](event: AsioEventID)
use @pony_asio_event_resubscribe_write[None](event: AsioEventID)
use @pony_asio_event_destroy[None](event: AsioEventID)
use @pony_asio_event_get_disposable[Bool](event: AsioEventID)
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
  on how to handle it. One is to inform the Pony runtime that it can no
  longer make progress and that runtime backpressure should be applied to
  any actors sending this one messages. For example, you might construct your
  application like:

  ```pony
  // Here we have a TCPConnectionNotify that upon construction
  // is given a BackpressureAuth token. This allows the notifier
  // to inform the Pony runtime when to apply and release backpressure
  // as the connection experiences it.
  // Note the calls to
  //
  // Backpressure.apply(_auth)
  // Backpressure.release(_auth)
  //
  // that apply and release backpressure as needed

  use "backpressure"
  use "collections"
  use "net"

  class SlowDown is TCPConnectionNotify
    let _auth: BackpressureAuth
    let _out: StdStream

    new iso create(auth: BackpressureAuth, out: StdStream) =>
      _auth = auth
      _out = out

    fun ref throttled(connection: TCPConnection ref) =>
      _out.print("Experiencing backpressure!")
      Backpressure.apply(_auth)

    fun ref unthrottled(connection: TCPConnection ref) =>
      _out.print("Releasing backpressure!")
      Backpressure.release(_auth)

    fun ref closed(connection: TCPConnection ref) =>
      // if backpressure has been applied, make sure we release
      // when shutting down
      _out.print("Releasing backpressure if applied!")
      Backpressure.release(_auth)

    fun ref connect_failed(conn: TCPConnection ref) =>
      None

  actor Main
    new create(env: Env) =>
      try
        let auth = env.root as AmbientAuth
        let socket = TCPConnection(auth, recover SlowDown(auth, env.out) end,
          "", "7669")
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
          recover ThrowItAway end, "", "7669")
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

  ## Proxy support

  Using the `proxy_via` callback in a `TCPConnectionNotify` it is possible
  to implement proxies. The function takes the intended destination host
  and service as parameters and returns a 2-tuple of the proxy host and
  service.

  The proxy `TCPConnectionNotify` should decorate another implementation of
  `TCPConnectionNotify` passing relevent data through.

  ### Example proxy implementation

  ```pony
  actor Main
    new create(env: Env) =>
      MyClient.create(
        "example.com", // we actually want to connect to this host
        "80",
        ExampleProxy.create("proxy.example.com", "80")) // we connect via this proxy

  actor MyClient
    new create(host: String, service: String, proxy: Proxy = NoProxy) =>
      let conn: TCPConnection = TCPConnection.create(
        env.root as AmbientAuth,
        proxy.apply(MyConnectionNotify.create()),
        host,
        service)

  class ExampleProxy is Proxy
    let _proxy_host: String
    let _proxy_service: String

    new create(proxy_host: String, proxy_service: String) =>
      _proxy_host = proxy_host
      _proxy_service = proxy_service

    fun apply(wrap: TCPConnectionNotify iso): TCPConnectionNotify iso^ =>
      ExampleProxyNotify.create(consume wrap, _proxy_service, _proxy_service)

  class iso ExampleProxyNotify is TCPConnectionNotify
    // Fictional proxy implementation that has no error
    // conditions, and always forwards the connection.
    let _proxy_host: String
    let _proxy_service: String
    var _destination_host: (None | String) = None
    var _destination_service: (None | String) = None
    let _wrapped: TCPConnectionNotify iso

    new iso create(wrap: TCPConnectionNotify iso, proxy_host: String, proxy_service: String) =>
      _wrapped = wrap
      _proxy_host = proxy_host
      _proxy_service = proxy_service

    fun ref proxy_via(host: String, service: String): (String, String) =>
      // Stash the original host & service; return the host & service
      // for the proxy; indicating that the initial TCP connection should
      // be made to the proxy
      _destination_host = host
      _destination_service = service
      (_proxy_host, _proxy_service)

    fun ref connected(conn: TCPConnection ref) =>
      // conn is the connection to the *proxy* server. We need to ask the
      // proxy server to forward this connection to our intended final
      // destination.
      conn.write((_destination_host + "\n").array())
      conn.write((_destination_service + "\n").array())
      wrapped.connected(conn)

    fun ref received(conn, data, times) => _wrapped.received(conn, data, times)
    fun ref connect_failed(conn: TCPConnection ref) => None
  ```

  """
  var _listen: (TCPListener | None) = None
  var _notify: TCPConnectionNotify
  var _connect_count: U32
  var _fd: U32 = -1
  var _event: AsioEventID = AsioEvent.none()
  var _connected: Bool = false
  var _readable: Bool = false
  var _reading: Bool = false
  var _writeable: Bool = false
  var _throttled: Bool = false
  var _closed: Bool = false
  var _shutdown: Bool = false
  var _shutdown_peer: Bool = false
  var _in_sent: Bool = false
  embed _pending_writev_posix: Array[(Pointer[U8] tag, USize)] = _pending_writev_posix.create()
  embed _pending_writev_windows: Array[(USize, Pointer[U8] tag)] = _pending_writev_windows.create()

  var _pending_sent: USize = 0
  var _pending_writev_total: USize = 0
  var _read_buf: Array[U8] iso
  var _read_buf_offset: USize = 0
  var _max_received_called: USize = 50

  let _read_buffer_size: USize

  let _yield_after_reading: USize
  let _yield_after_writing: USize

  var _expect: USize = 0

  var _muted: Bool = false

  new create(
    auth: TCPConnectionAuth,
    notify: TCPConnectionNotify iso,
    host: String,
    service: String,
    from: String = "",
    read_buffer_size: USize = 16384,
    yield_after_reading: USize = 16384,
    yield_after_writing: USize = 16384)
  =>
    """
    Connect via IPv4 or IPv6. If `from` is a non-empty string, the connection
    will be made from the specified interface.
    """
    _read_buf = recover Array[U8] .> undefined(read_buffer_size) end
    _read_buffer_size = read_buffer_size
    _yield_after_reading = yield_after_reading
    _yield_after_writing = yield_after_writing
    _notify = consume notify
    let asio_flags =
      ifdef not windows then
        AsioEvent.read_write_oneshot()
      else
        AsioEvent.read_write()
      end
    (let host', let service') = _notify.proxy_via(host, service)
    _connect_count =
      @pony_os_connect_tcp[U32](this, host'.cstring(), service'.cstring(),
        from.cstring(), asio_flags)
    _notify_connecting()

  new ip4(
    auth: TCPConnectionAuth,
    notify: TCPConnectionNotify iso,
    host: String,
    service: String,
    from: String = "",
    read_buffer_size: USize = 16384,
    yield_after_reading: USize = 16384,
    yield_after_writing: USize = 16384)
  =>
    """
    Connect via IPv4.
    """
    _read_buf = recover Array[U8] .> undefined(read_buffer_size) end
    _read_buffer_size = read_buffer_size
    _yield_after_reading = yield_after_reading
    _yield_after_writing = yield_after_writing
    _notify = consume notify
    let asio_flags =
      ifdef not windows then
        AsioEvent.read_write_oneshot()
      else
        AsioEvent.read_write()
      end
    (let host', let service') = _notify.proxy_via(host, service)
    _connect_count =
      @pony_os_connect_tcp4[U32](this, host'.cstring(), service'.cstring(),
        from.cstring(), asio_flags)
    _notify_connecting()

  new ip6(
    auth: TCPConnectionAuth,
    notify: TCPConnectionNotify iso,
    host: String,
    service: String,
    from: String = "",
    read_buffer_size: USize = 16384,
    yield_after_reading: USize = 16384,
    yield_after_writing: USize = 16384)
  =>
    """
    Connect via IPv6.
    """
    _read_buf = recover Array[U8] .> undefined(read_buffer_size) end
    _read_buffer_size = read_buffer_size
    _yield_after_reading = yield_after_reading
    _yield_after_writing = yield_after_writing
    _notify = consume notify
    let asio_flags =
      ifdef not windows then
        AsioEvent.read_write_oneshot()
      else
        AsioEvent.read_write()
      end
    (let host', let service') = _notify.proxy_via(host, service)
    _connect_count =
      @pony_os_connect_tcp6[U32](this, host'.cstring(), service'.cstring(),
        from.cstring(), asio_flags)
    _notify_connecting()

  new _accept(
    listen: TCPListener,
    notify: TCPConnectionNotify iso,
    fd: U32,
    read_buffer_size: USize = 16384,
    yield_after_reading: USize = 16384,
    yield_after_writing: USize = 16384)
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
    _read_buf = recover Array[U8] .> undefined(read_buffer_size) end
    _read_buffer_size = read_buffer_size
    _yield_after_reading = yield_after_reading
    _yield_after_writing = yield_after_writing

    _notify.accepted(this)

    _readable = true
    _queue_read()
    _pending_reads()

  be write[E: StringEncoder val = UTF8StringEncoder](data: (String | ByteSeq)) =>
    """
    Write a single sequence of bytes. Data will be silently discarded if the
    connection has not yet been established though.
    """
    if _connected and not _closed then
      _in_sent = true
      match data
      | let s: String =>
        write_final(_notify.sent(this, s.array[E]()))
      | let b: ByteSeq =>
        write_final(_notify.sent(this, b))
      end
      _in_sent = false
    end

  be writev[E: StringEncoder val = UTF8StringEncoder](data: (StringIter | ByteSeqIter)) =>
    """
    Write a sequence of sequences of bytes. Data will be silently discarded if
    the connection has not yet been established though.
    """
    if _connected and not _closed then
      _in_sent = true

      let byteArray = recover val
        let ba = Array[ByteSeq]
        match data
        | let si: StringIter =>
          for s in si.values() do
            ba.push(s.array[E]())
          end
        | let bsi: ByteSeqIter =>
          ba .> concat(bsi.values())
        end
        ba
      end

      ifdef windows then
        try
          var num_to_send: I32 = 0
          for bytes in _notify.sentv(this, byteArray).values() do
            // don't sent 0 byte payloads; windows doesn't like it (and it's wasteful)
            if bytes.size() == 0 then
              continue
            end

            // Add an IOCP write.
            _pending_writev_windows
              .> push((bytes.size(), bytes.cpointer()))
            _pending_writev_total = _pending_writev_total + bytes.size()
            num_to_send = num_to_send + 1
          end

          // Write as much data as possible.
          var len =
            @pony_os_writev[USize](_event,
              _pending_writev_windows.cpointer(_pending_sent),
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
        for bytes in _notify.sentv(this, byteArray).values() do
          // don't sent 0 byte payloads; it's wasteful
          if bytes.size() == 0 then
            continue
          end

          _pending_writev_posix
            .> push((bytes.cpointer(), bytes.size()))
          _pending_writev_total = _pending_writev_total + bytes.size()
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
    if not _reading then
      _pending_reads()
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

  fun local_address(): NetAddress =>
    """
    Return the local IP address. If this TCPConnection is closed then the
    address returned is invalid.
    """
    let ip = recover NetAddress end
    @pony_os_sockname[Bool](_fd, ip)
    ip

  fun remote_address(): NetAddress =>
    """
    Return the remote IP address. If this TCPConnection is closed then the
    address returned is invalid.
    """
    let ip = recover NetAddress end
    @pony_os_peername[Bool](_fd, ip)
    ip

  fun ref expect(qty: USize = 0) ? =>
    """
    A `received` call on the notifier must contain exactly `qty` bytes. If
    `qty` is zero, the call can contain any amount of data. This has no effect
    if called in the `sent` notifier callback.

    Errors if `qty` exceeds the max buffer size as indicated by the
    `read_buffer_size` supplied when the connection was created.
    """

    if qty <= _read_buffer_size then
      if not _in_sent then
        _expect = _notify.expect(this, qty)
        _read_buf_size()
      end
    else
      error
    end

  fun ref set_nodelay(state: Bool) =>
    """
    Turn Nagle on/off. Defaults to on. This can only be set on a connected
    socket.
    """
    if _connected then
      set_tcp_nodelay(state)
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
          if _is_sock_connected(fd) then
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
          // There is a possibility that a non-Windows system has
          // already unsubscribed this event already.  (Windows might
          // be vulnerable to this race, too, I'm not sure.) It's a
          // bug to do a second time.  Look at the disposable status
          // of the event (not the flags that this behavior's args!)
          // to see if it's ok to unsubscribe.
          if not @pony_asio_event_get_disposable(event) then
            @pony_asio_event_unsubscribe(event)
          end
          @pony_os_socket_close[None](fd)
          _try_shutdown()
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

  fun ref write_final[E: StringEncoder val = UTF8StringEncoder](data: (String | ByteSeq)) =>
    """
    Write as much as possible to the socket. Set `_writeable` to `false` if not
    everything was written. On an error, close the connection. This is for data
    that has already been transformed by the notifier. Data will be silently
    discarded if the connection has not yet been established though.
    """
    // don't sent 0 byte payloads; windows doesn't like it (and it's wasteful)
    if data.size() == 0 then
      return
    end

    if _connected and not _closed then
      ifdef windows then
        try
          // Add an IOCP write.
          match data
          | let s: String =>
            let a: Array[U8] val = s.array[E]()
            _pending_writev_windows .> push((a.size(), a.cpointer()))
            _pending_writev_total = _pending_writev_total + a.size()
          else
            _pending_writev_windows .> push((data.size(), data.cpointer()))
            _pending_writev_total = _pending_writev_total + data.size()
          end
          @pony_os_writev[USize](_event,
            _pending_writev_windows.cpointer(_pending_sent), I32(1)) ?

          _pending_sent = _pending_sent + 1

          if _pending_sent > 32 then
            // If more than 32 asynchronous writes are scheduled, apply
            // backpressure. The choice of 32 is rather arbitrary an
            // probably needs tuning
            _apply_backpressure()
          end
        end
      else
        match data
        | let s: String =>
          let a: Array[U8] val = s.array[E]()
          _pending_writev_posix .> push((a.cpointer(), a.size()))
          _pending_writev_total = _pending_writev_total + a.size()
        else
          _pending_writev_posix .> push((data.cpointer(), data.size()))
          _pending_writev_total = _pending_writev_total + data.size()
        end
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
        hard_close()
        return
      end

      try
        _manage_pending_buffer(len.usize(),
          _pending_writev_total, _pending_writev_windows.size())?
      end

      if _pending_sent < 16 then
        // If fewer than 16 asynchronous writes are scheduled, remove
        // backpressure. The choice of 16 is rather arbitrary and probably
        // needs to be tuned.
        _release_backpressure()
      end
    end

  be _write_again() =>
    """
    Resume writing.
    """
    _pending_writes()

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
      var bytes_sent: USize = 0
      while _writeable and (_pending_writev_total > 0) do
        if bytes_sent >= _yield_after_writing then
          // We've written _yield_after_writing bytes.
          // Yield and write again later.
          _write_again()
          return false
        end
        try
          // Determine number of bytes and buffers to send.
          if _pending_writev_posix.size() < writev_batch_size then
            num_to_send = _pending_writev_posix.size()
            bytes_to_send = _pending_writev_total
          else
            // Have more buffers than a single writev can handle.
            // Iterate over buffers being sent to add up total.
            num_to_send = writev_batch_size
            bytes_to_send = 0
            for d in Range[USize](0, num_to_send, 1) do
              bytes_to_send = bytes_to_send + _pending_writev_posix(d)?._2
            end
          end

          // Write as much data as possible.
          var len = @pony_os_writev[USize](_event,
            _pending_writev_posix.cpointer(), num_to_send.i32()) ?

          if _manage_pending_buffer(len, bytes_to_send, num_to_send)? then
            return true
          end

          bytes_sent = bytes_sent + len
        else
          // Non-graceful shutdown on error.
          hard_close()
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
      var num_sent: USize = 0
      while len > 0 do
        (let iov_p, let iov_s) =
          ifdef windows then
            (let tmp_s, let tmp_p) = _pending_writev_windows(num_sent)?
            (tmp_p, tmp_s)
          else
            _pending_writev_posix(num_sent)?
          end
        if iov_s <= len then
          num_sent = num_sent + 1
          len = len - iov_s
          _pending_writev_total = _pending_writev_total - iov_s
        else
          ifdef windows then
            _pending_writev_windows(num_sent)? = (iov_s-len, iov_p.offset(len))
          else
            _pending_writev_posix(num_sent)? = (iov_p.offset(len), iov_s-len)
          end
          _pending_writev_total = _pending_writev_total - len
          len = 0
        end
      end

      ifdef windows then
        // do a trim in place instead of many shifts for efficiency
        _pending_writev_windows.trim_in_place(num_sent)
        _pending_sent = _pending_sent - num_sent
      else
        // do a trim in place instead of many shifts for efficiency
        _pending_writev_posix.trim_in_place(num_sent)
      end

      ifdef not windows then
        _apply_backpressure()
      end
    else
      // sent all data we requested in this batch
      _pending_writev_total = _pending_writev_total - bytes_to_send
      if _pending_writev_total == 0 then
        ifdef windows then
          // do a trim in place instead of a clear to free up memory
          _pending_writev_windows.trim_in_place(_pending_writev_windows.size())
          _pending_sent = 0
        else
          // do a trim in place instead of a clear to free up memory
          _pending_writev_posix.trim_in_place(_pending_writev_posix.size())
        end
        return true
      else
        ifdef windows then
          // do a trim in place instead of many shifts for efficiency
          _pending_writev_windows.trim_in_place(num_to_send)
          _pending_sent = _pending_sent - num_to_send
        else
          // do a trim in place instead of many shifts for efficiency
          _pending_writev_posix.trim_in_place(num_to_send)
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
      if len == 0 then
        // The socket has been closed from the other side, or a hard close has
        // cancelled the queued read.
        _readable = false
        _shutdown_peer = true
        close()
        return
      end

      _read_buf_offset = _read_buf_offset + len.usize()

      while (not _muted) and (_read_buf_offset >= _expect)
        and (_read_buf_offset > 0) do
        // get data to be distributed and update `_read_buf_offset`
        let chop_at = if _expect == 0 then _read_buf_offset else _expect end
        (let data, _read_buf) = (consume _read_buf).chop(chop_at)
        _read_buf_offset = _read_buf_offset - chop_at

        _notify.received(this, consume data, 1)
        _read_buf_size()
      end

      _queue_read()
    end

  fun ref _read_buf_size() =>
    """
    Resize the read buffer if it is empty or smaller than the next payload size
    """
    if _read_buf.size() <= _expect then
      _read_buf.undefined(_read_buffer_size)
    end

  fun ref _queue_read() =>
    """
    Begin an IOCP read on Windows.
    """
    ifdef windows then
      try
        @pony_os_recv[USize](
          _event,
          _read_buf.cpointer(_read_buf_offset),
          _read_buf.size() - _read_buf_offset) ?
      else
        hard_close()
      end
    end

  fun ref _pending_reads() =>
    """
    Unless this connection is currently muted, read while data is available,
    guessing the next packet length as we go. If we read 5 kb of data, send
    ourself a resume message and stop reading, to avoid starving other actors.
    Currently we can handle a varying value of _expect (greater than 0) and
    constant _expect of 0 but we cannot handle switching between these two
    cases.
    """
    ifdef not windows then
      try
        var sum: USize = 0
        var received_called: USize = 0
        _reading = true

        while _readable and not _shutdown_peer do
          // exit if muted
          if _muted then
            _reading = false
            return
          end

          // distribute the data we've already read that is in the `read_buf`
          // and able to be distributed
          while (_read_buf_offset >= _expect) and (_read_buf_offset > 0) do
            // get data to be distributed and update `_read_buf_offset`
            let chop_at = if _expect == 0 then _read_buf_offset else _expect end
            (let data, _read_buf) = (consume _read_buf).chop(chop_at)
            _read_buf_offset = _read_buf_offset - chop_at

            // increment max reads
            received_called = received_called + 1

            // check if we should yield to let another actor run
            if (not _notify.received(this, consume data,
              received_called))
              or (received_called >= _max_received_called)
            then
              _read_again()
              _reading = false
              return
            end
          end

          if sum >= _yield_after_reading then
            // If we've read _yield_after_reading bytes
            // yield and read again later.
            _read_again()
            _reading = false
            return
          end

          _read_buf_size()

          // Read as much data as possible.
          let len = @pony_os_recv[USize](
            _event,
            _read_buf.cpointer(_read_buf_offset),
            _read_buf.size() - _read_buf_offset) ?

          if len == 0 then
            // Would block, try again later.
            // this is safe because asio thread isn't currently subscribed
            // for a read event so will not be writing to the readable flag
            @pony_asio_event_set_readable[None](_event, false)
            _readable = false
            _reading = false
            @pony_asio_event_resubscribe_read(_event)
            return
          end

          _read_buf_offset = _read_buf_offset + len
          sum = sum + len
        end
      else
        // The socket has been closed from the other side.
        _shutdown_peer = true
        hard_close()
      end
    end

    _reading = false

  fun ref _notify_connecting() =>
    """
    Inform the notifier that we're connecting.
    """
    if _connect_count > 0 then
      _notify.connecting(this, _connect_count)
    else
      _notify.connect_failed(this)
      hard_close()
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
        hard_close()
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
      hard_close()
    end

    ifdef windows then
      // On windows, wait until all outstanding IOCP operations have completed
      // or been cancelled.
      if not _connected and not _readable and (_pending_sent == 0) then
        @pony_asio_event_unsubscribe(_event)
      end
    end

  fun ref hard_close() =>
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

    _pending_writev_total = 0
    ifdef windows then
      _pending_writev_windows.clear()
      _pending_sent = 0
    else
      _pending_writev_posix.clear()
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


  // Check this when a connection gets its first writeable event.
  fun _is_sock_connected(fd: U32): Bool =>
    (let errno: U32, let value: U32) = _OSSocket.get_so_error(fd)
    (errno == 0) and (value == 0)

  fun ref _apply_backpressure() =>
    if not _throttled then
      _throttled = true
      _notify.throttled(this)
    end
    ifdef not windows then
      _writeable = false

      // this is safe because asio thread isn't currently subscribed
      // for a write event so will not be writing to the readable flag
      @pony_asio_event_set_writeable(_event, false)
      @pony_asio_event_resubscribe_write(_event)
    end

  fun ref _release_backpressure() =>
    if _throttled then
      _throttled = false
      _notify.unthrottled(this)
    end

  /**************************************/

  fun ref getsockopt(level: I32, option_name: I32, option_max_size: USize = 4):
    (U32, Array[U8] iso^) =>
    """
    General wrapper for TCP sockets to the `getsockopt(2)` system call.

    The caller must provide an array that is pre-allocated to be
    at least as large as the largest data structure that the kernel
    may return for the requested option.

    In case of system call success, this function returns the 2-tuple:
    1. The integer `0`.
    2. An `Array[U8]` of data returned by the system call's `void *`
       4th argument.  Its size is specified by the kernel via the
       system call's `sockopt_len_t *` 5th argument.

    In case of system call failure, this function returns the 2-tuple:
    1. The value of `errno`.
    2. An undefined value that must be ignored.

    Usage example:

    ```pony
    // connected() is a callback function for class TCPConnectionNotify
    fun ref connected(conn: TCPConnection ref) =>
      match conn.getsockopt(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf(), 4)
        | (0, let gbytes: Array[U8] iso) =>
          try
            let br = Reader.create().>append(consume gbytes)
            ifdef littleendian then
              let buffer_size = br.u32_le()?
            else
              let buffer_size = br.u32_be()?
            end
          end
        | (let errno: U32, _) =>
          // System call failed
      end
    ```
    """
    _OSSocket.getsockopt(_fd, level, option_name, option_max_size)

  fun ref getsockopt_u32(level: I32, option_name: I32): (U32, U32) =>
    """
    Wrapper for TCP sockets to the `getsockopt(2)` system call where
    the kernel's returned option value is a C `uint32_t` type / Pony
    type `U32`.

    In case of system call success, this function returns the 2-tuple:
    1. The integer `0`.
    2. The `*option_value` returned by the kernel converted to a Pony `U32`.

    In case of system call failure, this function returns the 2-tuple:
    1. The value of `errno`.
    2. An undefined value that must be ignored.
    """
    _OSSocket.getsockopt_u32(_fd, level, option_name)

  fun ref setsockopt(level: I32, option_name: I32, option: Array[U8]): U32 =>
    """
    General wrapper for TCP sockets to the `setsockopt(2)` system call.

    The caller is responsible for the correct size and byte contents of
    the `option` array for the requested `level` and `option_name`,
    including using the appropriate machine endian byte order.

    This function returns `0` on success, else the value of `errno` on
    failure.

    Usage example:

    ```pony
    // connected() is a callback function for class TCPConnectionNotify
    fun ref connected(conn: TCPConnection ref) =>
      let sb = Writer

      sb.u32_le(7744)             // Our desired socket buffer size
      let sbytes = Array[U8]
      for bs in sb.done().values() do
        sbytes.append(bs)
      end
      match conn.setsockopt(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf(), sbytes)
        | 0 =>
          // System call was successful
        | let errno: U32 =>
          // System call failed
      end
    ```
    """
    _OSSocket.setsockopt(_fd, level, option_name, option)

  fun ref setsockopt_u32(level: I32, option_name: I32, option: U32): U32 =>
    """
    General wrapper for TCP sockets to the `setsockopt(2)` system call where
    the kernel expects an option value of a C `uint32_t` type / Pony
    type `U32`.

    This function returns `0` on success, else the value of `errno` on
    failure.
    """
    _OSSocket.setsockopt_u32(_fd, level, option_name, option)


  fun ref get_so_error(): (U32, U32) =>
    """
    Wrapper for the FFI call `getsockopt(fd, SOL_SOCKET, SO_ERROR, ...)`
    """
    _OSSocket.get_so_error(_fd)

  fun ref get_so_rcvbuf(): (U32, U32) =>
    """
    Wrapper for the FFI call `getsockopt(fd, SOL_SOCKET, SO_RCVBUF, ...)`
    """
    _OSSocket.get_so_rcvbuf(_fd)

  fun ref get_so_sndbuf(): (U32, U32) =>
    """
    Wrapper for the FFI call `getsockopt(fd, SOL_SOCKET, SO_SNDBUF, ...)`
    """
    _OSSocket.get_so_sndbuf(_fd)

  fun ref get_tcp_nodelay(): (U32, U32) =>
    """
    Wrapper for the FFI call `getsockopt(fd, SOL_SOCKET, TCP_NODELAY, ...)`
    """
    _OSSocket.getsockopt_u32(_fd, OSSockOpt.sol_socket(), OSSockOpt.tcp_nodelay())


  fun ref set_so_rcvbuf(bufsize: U32): U32 =>
    """
    Wrapper for the FFI call `setsockopt(fd, SOL_SOCKET, SO_RCVBUF, ...)`
    """
    _OSSocket.set_so_rcvbuf(_fd, bufsize)

  fun ref set_so_sndbuf(bufsize: U32): U32 =>
    """
    Wrapper for the FFI call `setsockopt(fd, SOL_SOCKET, SO_SNDBUF, ...)`
    """
    _OSSocket.set_so_sndbuf(_fd, bufsize)

  fun ref set_tcp_nodelay(state: Bool): U32 =>
    """
    Wrapper for the FFI call `setsockopt(fd, SOL_SOCKET, TCP_NODELAY, ...)`
    """
    var word: Array[U8] ref =
      _OSSocket.u32_to_bytes4(if state then 1 else 0 end)
    _OSSocket.setsockopt(_fd, OSSockOpt.sol_socket(), OSSockOpt.tcp_nodelay(), word)
