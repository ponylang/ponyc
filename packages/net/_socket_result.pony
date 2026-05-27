// Pony-side dual of `pony_socket_result_t` defined in
// `src/libponyrt/lang/socket.h`. The integer values returned from `apply()`
// must match the C-side `PONY_SOCKET_OK`/`PONY_SOCKET_RETRY`/`PONY_SOCKET_ERROR`
// constants, which are part of the FFI ABI for the five `pony_os_*` socket
// runtime functions. Keep both files in sync.
//
// `_SocketResultDecoder` collapses any out-of-range U8 to `_SocketResultError`
// so unknown C-side values fail closed. Adding a new wire value on the C side
// requires updating both the `_SocketResult` union and this decoder.

primitive _SocketResultOk
  fun apply(): U8 => 0

primitive _SocketResultRetry
  fun apply(): U8 => 1

primitive _SocketResultError
  fun apply(): U8 => 2

type _SocketResult is
  (_SocketResultOk | _SocketResultRetry | _SocketResultError)

primitive _SocketResultDecoder
  fun apply(v: U8): _SocketResult =>
    match v
    | _SocketResultOk() => _SocketResultOk
    | _SocketResultRetry() => _SocketResultRetry
    else _SocketResultError
    end
