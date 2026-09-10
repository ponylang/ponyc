trait _UDPSocketState[UDP: UDPBackend ref]
  """
  One state in the UDP socket lifecycle. `UDPSocket._state` holds the current
  one, and lifecycle-gated operations dispatch through it.
  """
  fun ref event_notify(sock: UDPSocket[UDP] ref, flags: U32)

  fun ref send_to(sock: UDPSocket[UDP] ref,
    data: ByteSeq,
    to: NetAddress box)
    : SendToResult

  fun ref close(sock: UDPSocket[UDP] ref)
  fun ref read_again(sock: UDPSocket[UDP] ref)

  fun getsockopt(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)

  fun getsockopt_u32(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)

  fun setsockopt(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32

  fun setsockopt_u32(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32

  fun is_open(): Bool
  fun is_closed(): Bool

class _UDPNone[UDP: UDPBackend ref] is _UDPSocketState[UDP]
  """
  Pre-initialization. The socket has not yet bound. Handles the dispose/init
  race: if `dispose()` arrives before `_finish_initialization()`, `close()`
  transitions to `_UDPClosed` so initialization sees it and skips.
  """
  fun ref event_notify(sock: UDPSocket[UDP] ref, flags: U32) =>
    _Unreachable()

  fun ref send_to(sock: UDPSocket[UDP] ref,
    data: ByteSeq,
    to: NetAddress box)
    : SendToResult
  =>
    SendToNotOpen

  fun ref close(sock: UDPSocket[UDP] ref) =>
    sock._set_state(_UDPClosed[UDP])

  fun ref read_again(sock: UDPSocket[UDP] ref) =>
    _Unreachable()

  fun getsockopt(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    (1, recover Array[U8] end)

  fun getsockopt_u32(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)
  =>
    (1, 0)

  fun setsockopt(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    1

  fun setsockopt_u32(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32
  =>
    1

  fun is_open(): Bool => false
  fun is_closed(): Bool => false

class _UDPOpen[UDP: UDPBackend ref] is _UDPSocketState[UDP]
  """
  Bound and active. I/O events are dispatched here.
  """
  fun ref event_notify(sock: UDPSocket[UDP] ref, flags: U32) =>
    sock._dispatch_io_event(flags)

  fun ref send_to(sock: UDPSocket[UDP] ref,
    data: ByteSeq,
    to: NetAddress box)
    : SendToResult
  =>
    sock._do_send_to(data, to)

  fun ref close(sock: UDPSocket[UDP] ref) =>
    sock._do_close()

  fun ref read_again(sock: UDPSocket[UDP] ref) =>
    sock._do_read_again()

  fun getsockopt(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    sock._do_getsockopt(level, option_name, option_max_size)

  fun getsockopt_u32(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)
  =>
    sock._do_getsockopt_u32(level, option_name)

  fun setsockopt(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    sock._do_setsockopt(level, option_name, option)

  fun setsockopt_u32(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32
  =>
    sock._do_setsockopt_u32(level, option_name, option)

  fun is_open(): Bool => true
  fun is_closed(): Bool => false

class _UDPClosed[UDP: UDPBackend ref] is _UDPSocketState[UDP]
  """
  Terminal state. The socket is closed.
  """
  fun ref event_notify(sock: UDPSocket[UDP] ref, flags: U32) =>
    None

  fun ref send_to(sock: UDPSocket[UDP] ref,
    data: ByteSeq,
    to: NetAddress box)
    : SendToResult
  =>
    SendToNotOpen

  fun ref close(sock: UDPSocket[UDP] ref) =>
    None

  fun ref read_again(sock: UDPSocket[UDP] ref) =>
    None

  fun getsockopt(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    (1, recover Array[U8] end)

  fun getsockopt_u32(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)
  =>
    (1, 0)

  fun setsockopt(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    1

  fun setsockopt_u32(sock: UDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32
  =>
    1

  fun is_open(): Bool => false
  fun is_closed(): Bool => true
