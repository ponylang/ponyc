use "collections"

use @pony_os_listen_udp[AsioEventID](owner: AsioEventNotify,
  host: Pointer[U8] tag, service: Pointer[U8] tag)
use @pony_os_listen_udp4[AsioEventID](owner: AsioEventNotify,
  host: Pointer[U8] tag, service: Pointer[U8] tag)
use @pony_os_listen_udp6[AsioEventID](owner: AsioEventNotify,
  host: Pointer[U8] tag, service: Pointer[U8] tag)
use @pony_os_sendto[U8](fd: U32, buffer: Pointer[U8] tag,
  size: USize, to: NetAddress tag, count_out: Pointer[USize])
use @pony_os_recvfrom[U8](event: AsioEventID, buffer: Pointer[U8] tag,
  size: USize, from: NetAddress tag, count_out: Pointer[USize])
use @pony_os_multicast_join[None](fd: U32, group: Pointer[U8] tag,
  to: Pointer[U8] tag)
use @pony_os_multicast_leave[None](fd: U32, group: Pointer[U8] tag,
  to: Pointer[U8] tag)
use @pony_os_multicast_interface[None](fd: U32, from: Pointer[U8] tag)

actor UDPSocket is AsioEventNotify
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
      UDPSocket(UDPAuth(env.root),
        MyUDPNotify, "", "8989")
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
          DNS.ip4(DNSAuth(env.root), "localhost", "8989")(0)?
        UDPSocket(UDPAuth(env.root),
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
    auth: UDPAuth,
    notify: UDPNotify iso,
    host: String = "",
    service: String = "0",
    size: USize = 1024)
  =>
    """
    Listens for datagrams. The address family is whichever the resolver
    returns first for `host`/`service`, so it depends on the environment;
    use `ip4` or `ip6` to pin a specific family.

    `size` is the read buffer size in bytes (default 1024) and therefore the
    maximum datagram length delivered to `UDPNotify.received`; a larger datagram
    is truncated to `size` and the excess is silently discarded (see
    `UDPNotify.received`).
    """
    _notify = consume notify
    _event =
      @pony_os_listen_udp(this, host.cstring(), service.cstring())
    _fd = @pony_asio_event_fd(_event)
    @pony_os_sockname(_fd, _ip)
    _packet_size = size
    _read_buf = recover Array[U8] .> undefined(size) end
    _notify_listening()
    _start_next_read()

  new ip4(
    auth: UDPAuth,
    notify: UDPNotify iso,
    host: String = "",
    service: String = "0",
    size: USize = 1024)
  =>
    """
    Listens for IPv4 datagrams.

    `size` is the read buffer size in bytes (default 1024) and therefore the
    maximum datagram length delivered to `UDPNotify.received`; a larger datagram
    is truncated to `size` and the excess is silently discarded (see
    `UDPNotify.received`).
    """
    _notify = consume notify
    _event =
      @pony_os_listen_udp4(this, host.cstring(), service.cstring())
    _fd = @pony_asio_event_fd(_event)
    @pony_os_sockname(_fd, _ip)
    _packet_size = size
    _read_buf = recover Array[U8] .> undefined(size) end
    _notify_listening()
    _start_next_read()

  new ip6(
    auth: UDPAuth,
    notify: UDPNotify iso,
    host: String = "",
    service: String = "0",
    size: USize = 1024)
  =>
    """
    Listens for IPv6 datagrams.

    `size` is the read buffer size in bytes (default 1024) and therefore the
    maximum datagram length delivered to `UDPNotify.received`; a larger datagram
    is truncated to `size` and the excess is silently discarded (see
    `UDPNotify.received`).
    """
    _notify = consume notify
    _event =
      @pony_os_listen_udp6(this, host.cstring(), service.cstring())
    _fd = @pony_asio_event_fd(_event)
    @pony_os_sockname(_fd, _ip)
    _packet_size = size
    _read_buf = recover Array[U8] .> undefined(size) end
    _notify_listening()
    _start_next_read()

  be write(data: ByteSeq, to: NetAddress) =>
    """
    Write a single sequence of bytes.
    """
    _write(data, to)

  be writev(data: ByteSeqIter, to: NetAddress) =>
    """
    Write a sequence of byte sequences. Each `ByteSeq` in `data` is sent
    as a separate UDP datagram; this method does not combine them into a
    single packet.

    Note that this differs from the POSIX `writev` system call, which
    for record-based protocols like UDP would produce a single datagram.
    If you need a single datagram, concatenate the data yourself and call
    `write`.
    """
    for bytes in data.values() do
      _write(bytes, to)
    end

  be set_notify(notify: UDPNotify iso) =>
    """
    Change the notifier.
    """
    _notify = consume notify

  be set_broadcast(state: Bool) =>
    """
    Enable or disable broadcasting from this socket. This sets the
    `SO_BROADCAST` socket option, a sender-side permission to send to
    IPv4 broadcast addresses (see `DNS.broadcast_ip4`).

    On an IPv6 socket this is a no-op: IPv6 has no broadcast. Sending to
    a multicast address such as the all-nodes group (see
    `DNS.broadcast_ip6`) requires no permission; to receive traffic for
    a multicast group, use `multicast_join`.

    The default constructor binds whichever address family the resolver
    returns first, so construct the socket with `ip4` if you need
    broadcast; otherwise `set_broadcast` may silently be a no-op.
    """
    if not _closed then
      if _ip.ip4() then
        set_so_broadcast(state)
      end
    end

  be set_multicast_interface(from: String = "") =>
    """
    By default, the OS will choose which address is used to send packets bound
    for multicast addresses. This can be used to force a specific interface.

    For an IPv4 interface, pass the interface's IPv4 address. For an IPv6
    interface, the interface is taken from the scope id of the resolved
    address, so only a scoped address such as `"fe80::1%eth0"` selects an
    interface; a plain IPv6 address does not.

    Calling with an empty string currently has no effect.
    """
    if not _closed then
      @pony_os_multicast_interface(_fd, from.cstring())
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
      @pony_os_multicast_join(_fd, group.cstring(), to.cstring())
    end

  be multicast_leave(group: String, to: String = "") =>
    """
    Drop a multicast group. This can be limited to packets arriving on a
    specific interface. No attempt is made to check that this socket has
    previously added this group.
    """
    if not _closed then
      @pony_os_multicast_leave(_fd, group.cstring(), to.cstring())
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
      if AsioEvent.errored(flags) then
        _close()
        return
      end

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
      @pony_asio_event_destroy(_event)
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
          var count: USize = 0
          match \exhaustive\ _SocketResultDecoder(
            @pony_os_recvfrom(_event, data.cpointer(), data.space(),
              from, addressof count))
          | _SocketResultOk =>
            data.truncate(count)
            _notify.received(this, consume data, consume from)

            sum = sum + count

            if sum > (1 << 12) then
              _read_again()
              return
            end
          | _SocketResultRetry =>
            _readable = false
            return
          | _SocketResultError => error
          end
        end
      else
        _close()
      end
    end

  fun ref _complete_reads(len: U32) =>
    """
    The OS has informed us that len bytes of pending reads have completed.
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
      // On a failed listen `_event` is null; the constructors still call us
      // unconditionally right after `_notify_listening`. Passing the null
      // event to `pony_os_recvfrom` is safe -- the runtime guards it and
      // returns an error (issue #5474) -- which lands in the error arm
      // below and closes the already-dead socket.
      //
      // `count` is unused on this path: Windows IOCP `pony_os_recvfrom`
      // returns OK with count=0 because the actual byte count arrives
      // asynchronously via `_complete_reads`. The local is required by
      // the FFI shape.
      //
      // `_SocketResultRetry` is unreachable here — `iocp_recvfrom` only
      // distinguishes "queued" from "failed" — but `\exhaustive\`
      // requires the arm. Treat it as failure to be safe.
      var count: USize = 0
      match \exhaustive\ _SocketResultDecoder(
        @pony_os_recvfrom(_event, _read_buf.cpointer(),
          _read_buf.space(), _read_from, addressof count))
      | _SocketResultOk => None
      | _SocketResultRetry =>
        _readable = false
        _close()
      | _SocketResultError =>
        _readable = false
        _close()
      end
    end

  fun ref _write(data: ByteSeq, to: NetAddress) =>
    """
    Write the datagram to the socket.
    """
    if not _closed then
      // On Windows, `count` is unused — IOCP `pony_os_sendto` returns OK
      // with count=0; the byte count arrives asynchronously. On POSIX,
      // `count` holds bytes sent on Ok but is also discarded here (UDP is
      // unreliable; we don't track partial-send progress). The local is
      // required by the FFI shape on both platforms.
      //
      // POSIX `_SocketResultRetry` (EWOULDBLOCK/EAGAIN) silently drops
      // the datagram, matching pre-change UDP semantics. Windows
      // IOCP `pony_os_sendto` cannot return Retry — `\exhaustive\`
      // requires the arm regardless.
      var count: USize = 0
      match \exhaustive\ _SocketResultDecoder(
        @pony_os_sendto(_fd, data.cpointer(), data.size(), to,
          addressof count))
      | _SocketResultOk => None
      | _SocketResultRetry => None
      | _SocketResultError => _close()
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
        @pony_asio_event_unsubscribe(_event)
      end
    else
      // Unsubscribe immediately.
      if not _event.is_null() then
        @pony_asio_event_unsubscribe(_event)
        _readable = false
      end
    end

    _closed = true

    if _fd != -1 then
      _notify.closed(this)
      // On windows, this will also cancel all outstanding IOCP operations.
      @pony_os_socket_close(_fd)
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
    Wrapper for the FFI call `setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, ...)`
    """
    var word: Array[U8] ref =
      _OSSocket.u32_to_bytes4(if loopback then 1 else 0 end)
    _OSSocket.setsockopt(_fd, OSSockOpt.ipproto_ip(), OSSockOpt.ip_multicast_loop(), word)

  fun ref set_ip_multicast_ttl(ttl: U8): U32 =>
    """
    Wrapper for the FFI call `setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, ...)`
    """
    var word: Array[U8] ref = _OSSocket.u32_to_bytes4(ttl.u32())
    _OSSocket.setsockopt(_fd, OSSockOpt.ipproto_ip(), OSSockOpt.ip_multicast_ttl(), word)

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
