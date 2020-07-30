use "collections"

type UDPSocketAuth is (AmbientAuth | NetAuth | UDPAuth)

actor UDPSocket
  """
  Creates a UDP socket that can be used for sending and receiving UDP messages.

  The following examples create:
  * an echo server that listens for connections and returns whatever message it
    receives
  * a client that connects to the server, sends a message, and prints the
    message it receives in response

  The server is implemented like this:

  ```pony
  use "net"

  class MyUDPNotify is UDPNotify
    fun ref received(
      sock: UDPSocket ref,
      data: Array[U8] iso,
      from: NetAddress)
    =>
      sock.write(consume data, from)

    fun ref not_listening(sock: UDPSocket ref) =>
      None

  actor Main
    new create(env: Env) =>
      try
        UDPSocket(env.root as AmbientAuth,
          MyUDPNotify, "", "8989")
      end
  ```

  The client is implemented like this:

  ```pony
  use "net"

  class MyUDPNotify is UDPNotify
    let _out: OutStream
    let _destination: NetAddress

    new create(
      out: OutStream,
      destination: NetAddress)
    =>
      _out = out
      _destination = destination

    fun ref listening(sock: UDPSocket ref) =>
      sock.write("hello world", _destination)

    fun ref received(
      sock: UDPSocket ref,
      data: Array[U8] iso,
      from: NetAddress)
    =>
      _out.print("GOT:" + String.from_array(consume data))
      sock.dispose()

    fun ref not_listening(sock: UDPSocket ref) =>
      None

  actor Main
    new create(env: Env) =>
      try
        let destination =
          DNS.ip4(env.root as AmbientAuth, "localhost", "8989")(0)?
        UDPSocket(env.root as AmbientAuth,
          recover MyUDPNotify(env.out, consume destination) end)
      end
  ```
  """
  var _notify: UDPNotify
  var _fd: U32
  var _event: AsioEventID
  var _readable: Bool = false
  var _closed: Bool = false
  var _packet_size: USize
  var _read_buf: Array[U8] iso
  var _read_from: NetAddress iso = NetAddress
  embed _ip: NetAddress = NetAddress

  new create(
    auth: UDPSocketAuth,
    notify: UDPNotify iso,
    host: String = "",
    service: String = "0",
    size: USize = 1024)
  =>
    """
    Listens for both IPv4 and IPv6 datagrams.
    """
    _notify = consume notify
    _event =
      @pony_os_listen_udp[AsioEventID](this,
        host.cstring(), service.cstring())
    _fd = @pony_asio_event_fd(_event)
    @pony_os_sockname[Bool](_fd, _ip)
    _packet_size = size
    _read_buf = recover Array[U8] .> undefined(size) end
    _notify_listening()
    _start_next_read()

  new ip4(
    auth: UDPSocketAuth,
    notify: UDPNotify iso,
    host: String = "",
    service: String = "0",
    size: USize = 1024)
  =>
    """
    Listens for IPv4 datagrams.
    """
    _notify = consume notify
    _event =
      @pony_os_listen_udp4[AsioEventID](this,
        host.cstring(), service.cstring())
    _fd = @pony_asio_event_fd(_event)
    @pony_os_sockname[Bool](_fd, _ip)
    _packet_size = size
    _read_buf = recover Array[U8] .> undefined(size) end
    _notify_listening()
    _start_next_read()

  new ip6(
    auth: UDPSocketAuth,
    notify: UDPNotify iso,
    host: String = "",
    service: String = "0",
    size: USize = 1024)
  =>
    """
    Listens for IPv6 datagrams.
    """
    _notify = consume notify
    _event =
      @pony_os_listen_udp6[AsioEventID](this,
        host.cstring(), service.cstring())
    _fd = @pony_asio_event_fd(_event)
    @pony_os_sockname[Bool](_fd, _ip)
    _packet_size = size
    _read_buf = recover Array[U8] .> undefined(size) end
    _notify_listening()
    _start_next_read()

  be write[E: StringEncoder val = UTF8StringEncoder](data: (String | ByteSeq), to: NetAddress) =>
    """
    Write a single sequence of bytes.
    """
    match data
    | let s: String =>
      _write(s.array[E](), to)
    | let b: ByteSeq =>
      _write(b, to)
    end

  be writev[E: StringEncoder val = UTF8StringEncoder](data: (StringIter | ByteSeqIter), to: NetAddress) =>
    """
    Write a sequence of sequences of bytes.
    """
    match data
    | let si: StringIter =>
      for s in si.values() do
        _write(s.array[E](), to)
      end
    | let bsi: ByteSeqIter =>
      for bytes in bsi.values() do
        _write(bytes, to)
      end
    end

  be set_notify(notify: UDPNotify iso) =>
    """
    Change the notifier.
    """
    _notify = consume notify

  be set_broadcast(state: Bool) =>
    """
    Enable or disable broadcasting from this socket.
    """
    if not _closed then
      if _ip.ip4() then
        set_so_broadcast(state)
      elseif _ip.ip6() then
        @pony_os_multicast_join[None](_fd, "FF02::1".cstring(), "".cstring())
      end
    end

  be set_multicast_interface(from: String = "") =>
    """
    By default, the OS will choose which address is used to send packets bound
    for multicast addresses. This can be used to force a specific interface. To
    revert to allowing the OS to choose, call with an empty string.
    """
    if not _closed then
      @pony_os_multicast_interface[None](_fd, from.cstring())
    end

  be set_multicast_loopback(loopback: Bool) =>
    """
    By default, packets sent to a multicast address will be received by the
    sending system if it has subscribed to that address. Disabling loopback
    prevents this.
    """
    if not _closed then
      set_ip_multicast_loop(loopback)
    end

  be set_multicast_ttl(ttl: U8) =>
    """
    Set the TTL for multicast sends. Defaults to 1.
    """
    if not _closed then
      set_ip_multicast_ttl(ttl)
    end

  be multicast_join(group: String, to: String = "") =>
    """
    Add a multicast group. This can be limited to packets arriving on a
    specific interface.
    """
    if not _closed then
      @pony_os_multicast_join[None](_fd, group.cstring(), to.cstring())
    end

  be multicast_leave(group: String, to: String = "") =>
    """
    Drop a multicast group. This can be limited to packets arriving on a
    specific interface. No attempt is made to check that this socket has
    previously added this group.
    """
    if not _closed then
      @pony_os_multicast_leave[None](_fd, group.cstring(), to.cstring())
    end

  be dispose() =>
    """
    Stop listening.
    """
    if not _closed then
      _close()
    end

  fun local_address(): NetAddress =>
    """
    Return the bound IP address.
    """
    _ip

  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    """
    When we are readable, we accept new connections until none remain.
    """
    if event isnt _event then
      return
    end

    if not _closed then
      if AsioEvent.readable(flags) then
        _readable = true
        _complete_reads(arg)
        _pending_reads()
      end
    else
      ifdef windows then
        if AsioEvent.readable(flags) then
          _readable = false
          _close()
        end
      end
    end

    if AsioEvent.disposable(flags) then
      @pony_asio_event_destroy[None](_event)
      _event = AsioEvent.none()
    end

  be _read_again() =>
    """
    Resume reading.
    """
    if not _closed then
      _pending_reads()
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

        while _readable do
          let size = _packet_size
          let data = _read_buf = recover Array[U8] .> undefined(size) end
          let from = recover NetAddress end
          let len =
            @pony_os_recvfrom[USize](_event, data.cpointer(), data.space(),
              from) ?

          if len == 0 then
            _readable = false
            return
          end

          data.truncate(len)
          _notify.received(this, consume data, consume from)

          sum = sum + len

          if sum > (1 << 12) then
            _read_again()
            return
          end
        end
      else
        _close()
      end
    end

  fun ref _complete_reads(len: U32) =>
    """
    The OS has informed as that len bytes of pending reads have completed.
    This occurs only with IOCP on Windows.
    """
    ifdef windows then
      if _read_buf.space() == 0 then
        // Socket has been closed
        _readable = false
        _close()
        return
      end

      if _closed then
        return
      end

      // Hand back read data
      let size = _packet_size
      let data = _read_buf = recover Array[U8] .> undefined(size) end
      let from = _read_from = recover NetAddress end
      data.truncate(len.usize())
      _notify.received(this, consume data, consume from)

      _start_next_read()
    end

  fun ref _start_next_read() =>
    """
    Start our next receive.
    This is used only with IOCP on Windows.
    """
    ifdef windows then
      try
        @pony_os_recvfrom[USize](_event, _read_buf.cpointer(),
          _read_buf.space(), _read_from) ?
      else
        _readable = false
        _close()
      end
    end

  fun ref _write(data: (ByteSeq), to: NetAddress) =>
    """
    Write the datagram to the socket.
    """
    if not _closed then
      try
        @pony_os_sendto[USize](_fd, data.cpointer(), data.size(), to) ?
      else
        _close()
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

  fun ref _close() =>
    """
    Inform the notifier that we've closed.
    """
    ifdef windows then
      // On windows, wait until IOCP read operation has completed or been
      // cancelled.
      if _closed and not _readable and not _event.is_null() then
        @pony_asio_event_unsubscribe[None](_event)
      end
    else
      // Unsubscribe immediately.
      if not _event.is_null() then
        @pony_asio_event_unsubscribe[None](_event)
        _readable = false
      end
    end

    _closed = true

    if _fd != -1 then
      _notify.closed(this)
      // On windows, this will also cancel all outstanding IOCP operations.
      @pony_os_socket_close[None](_fd)
      _fd = -1
    end

  fun ref getsockopt(level: I32, option_name: I32, option_max_size: USize = 4): (U32, Array[U8] iso^) =>
    """
    General wrapper for UDP sockets to the `getsockopt(2)` system call.

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
    // listening() is a callback function for class UDPNotify
    fun ref listening(sock: UDPSocket ref) =>
      match sock.getsockopt(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf(), 4)
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
    Wrapper for UDP sockets to the `getsockopt(2)` system call where
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
    General wrapper for UDP sockets to the `setsockopt(2)` system call.

    The caller is responsible for the correct size and byte contents of
    the `option` array for the requested `level` and `option_name`,
    including using the appropriate CPU endian byte order.

    This function returns `0` on success, else the value of `errno` on
    failure.

    Usage example:

    ```pony
    // listening() is a callback function for class UDPNotify
    fun ref listening(sock: UDPSocket ref) =>
      let sb = Writer

      sb.u32_le(7744)             // Our desired socket buffer size
      let sbytes = Array[U8]
      for bs in sb.done().values() do
        sbytes.append(bs)
      end
      match sock.setsockopt(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf(), sbytes)
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
    Wrapper for UDP sockets to the `setsockopt(2)` system call where
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


  fun ref set_ip_multicast_loop(loopback: Bool): U32 =>
    """
    Wrapper for the FFI call `setsockopt(fd, SOL_SOCKET, IP_MULTICAST_LOOP, ...)`
    """
    var word: Array[U8] ref =
      _OSSocket.u32_to_bytes4(if loopback then 1 else 0 end)
    _OSSocket.setsockopt(_fd, OSSockOpt.sol_socket(), OSSockOpt.ip_multicast_loop(), word)

  fun ref set_ip_multicast_ttl(ttl: U8): U32 =>
    """
    Wrapper for the FFI call `setsockopt(fd, SOL_SOCKET, IP_MULTICAST_TTL, ...)`
    """
    var word: Array[U8] ref = _OSSocket.u32_to_bytes4(ttl.u32())
    _OSSocket.setsockopt(_fd, OSSockOpt.sol_socket(), OSSockOpt.ip_multicast_ttl(), word)

  fun ref set_so_broadcast(state: Bool): U32 =>
    """
    Wrapper for the FFI call `setsockopt(fd, SOL_SOCKET, SO_BROADCAST, ...)`
    """
    var word: Array[U8] ref =
      _OSSocket.u32_to_bytes4(if state then 1 else 0 end)
    _OSSocket.setsockopt(_fd, OSSockOpt.sol_socket(), OSSockOpt.so_broadcast(), word)

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
