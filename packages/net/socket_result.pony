primitive SocketResultOk
  """
  The socket operation completed. For a send, the runtime accepted some
  bytes; for a recv, bytes were read into the supplied buffer. The accompanying
  count is the number of bytes handled. The operation is synchronous and
  non-blocking on every platform (the Windows backend uses readiness
  notifications, not overlapped IOCP), so the count is always the bytes
  transferred by this call.
  """
  fun apply(): U8 => 0

primitive SocketResultRetry
  """
  The operation could not proceed without blocking (POSIX `EWOULDBLOCK`/
  `EAGAIN`, Windows `WSAEWOULDBLOCK`). No bytes were transferred. The
  caller should wait for a readiness event from ASIO and try again.
  """
  fun apply(): U8 => 1

primitive SocketResultError
  """
  An unrecoverable error occurred, or the peer closed the connection
  (POSIX `recv` returning 0 is mapped here so the runtime never reports OK
  with a 0-byte read). The socket should be closed.
  """
  fun apply(): U8 => 2

type SocketResult is
  ( SocketResultOk
  | SocketResultRetry
  | SocketResultError )

primitive SocketResultDecoder
  """
  Decodes the `U8` returned by the six `pony_os_*` socket runtime functions
  (`pony_os_writev`, `pony_os_sendv`, `pony_os_send`, `pony_os_recv`,
  `pony_os_sendto`, `pony_os_recvfrom`) into a `SocketResult` union.

  This is the Pony-side dual of `pony_socket_result_t` defined in
  `src/libponyrt/lang/socket.h` of ponyc. The integer values produced by
  the `SocketResultOk`/`SocketResultRetry`/`SocketResultError` primitives'
  `apply()` methods must match the C-side `PONY_SOCKET_OK`/
  `PONY_SOCKET_RETRY`/`PONY_SOCKET_ERROR` constants, which are part of the
  FFI ABI. Keep both files in sync.

  Any out-of-range `U8` is collapsed to `SocketResultError` so unknown
  C-side values fail closed. Adding a new wire value on the C side
  requires updating both the `SocketResult` union and this decoder.
  """
  fun apply(v: U8): SocketResult =>
    match v
    | SocketResultOk() => SocketResultOk
    | SocketResultRetry() => SocketResultRetry
    else SocketResultError
    end
