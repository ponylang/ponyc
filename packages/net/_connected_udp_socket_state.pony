trait _ConnectedUDPSocketState[UDP: ConnectedUDPBackend ref]
  """
  One state in the connected UDP socket lifecycle.
  `ConnectedUDPSocket._state` holds the current one, and lifecycle-gated
  operations dispatch through it.
  """
  fun ref event_notify(sock: ConnectedUDPSocket[UDP] ref, flags: U32)

  fun ref send(sock: ConnectedUDPSocket[UDP] ref, data: ByteSeq): UDPSendResult

  fun ref close(sock: ConnectedUDPSocket[UDP] ref)
  fun ref read_again(sock: ConnectedUDPSocket[UDP] ref)

  fun getsockopt(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)

  fun getsockopt_u32(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)

  fun setsockopt(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32

  fun setsockopt_u32(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32

  fun is_open(): Bool
  fun is_closed(): Bool

class _ConnectedUDPNone[UDP: ConnectedUDPBackend ref]
  is _ConnectedUDPSocketState[UDP]
  """
  Pre-initialization. The socket has not yet bound or connected. Handles the
  dispose/init race: if `dispose()` arrives before `_finish_initialization()`,
  `close()` transitions to `_ConnectedUDPClosed` so initialization sees it
  and skips.
  """
  fun ref event_notify(sock: ConnectedUDPSocket[UDP] ref, flags: U32) =>
    _Unreachable()

  fun ref send(sock: ConnectedUDPSocket[UDP] ref, data: ByteSeq)
    : UDPSendResult
  =>
    UDPSendNotOpen

  fun ref close(sock: ConnectedUDPSocket[UDP] ref) =>
    sock._set_state(_ConnectedUDPClosed[UDP])

  fun ref read_again(sock: ConnectedUDPSocket[UDP] ref) =>
    _Unreachable()

  fun getsockopt(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    (1, recover Array[U8] end)

  fun getsockopt_u32(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)
  =>
    (1, 0)

  fun setsockopt(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    1

  fun setsockopt_u32(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32
  =>
    1

  fun is_open(): Bool => false
  fun is_closed(): Bool => false

class _ConnectedUDPOpen[UDP: ConnectedUDPBackend ref]
  is _ConnectedUDPSocketState[UDP]
  """
  Bound, connected, and active. I/O events are dispatched here.
  """
  fun ref event_notify(sock: ConnectedUDPSocket[UDP] ref, flags: U32) =>
    sock._dispatch_io_event(flags)

  fun ref send(sock: ConnectedUDPSocket[UDP] ref, data: ByteSeq)
    : UDPSendResult
  =>
    sock._do_send(data)

  fun ref close(sock: ConnectedUDPSocket[UDP] ref) =>
    sock._do_close()

  fun ref read_again(sock: ConnectedUDPSocket[UDP] ref) =>
    sock._do_read_again()

  fun getsockopt(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    sock._do_getsockopt(level, option_name, option_max_size)

  fun getsockopt_u32(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)
  =>
    sock._do_getsockopt_u32(level, option_name)

  fun setsockopt(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    sock._do_setsockopt(level, option_name, option)

  fun setsockopt_u32(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32
  =>
    sock._do_setsockopt_u32(level, option_name, option)

  fun is_open(): Bool => true
  fun is_closed(): Bool => false

class _ConnectedUDPClosed[UDP: ConnectedUDPBackend ref]
  is _ConnectedUDPSocketState[UDP]
  """
  Terminal state. The socket is closed.
  """
  fun ref event_notify(sock: ConnectedUDPSocket[UDP] ref, flags: U32) =>
    None

  fun ref send(sock: ConnectedUDPSocket[UDP] ref, data: ByteSeq)
    : UDPSendResult
  =>
    UDPSendNotOpen

  fun ref close(sock: ConnectedUDPSocket[UDP] ref) =>
    None

  fun ref read_again(sock: ConnectedUDPSocket[UDP] ref) =>
    None

  fun getsockopt(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option_max_size: USize)
    : (U32, Array[U8] iso^)
  =>
    (1, recover Array[U8] end)

  fun getsockopt_u32(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32)
    : (U32, U32)
  =>
    (1, 0)

  fun setsockopt(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: Array[U8])
    : U32
  =>
    1

  fun setsockopt_u32(sock: ConnectedUDPSocket[UDP] box,
    level: I32,
    option_name: I32,
    option: U32)
    : U32
  =>
    1

  fun is_open(): Bool => false
  fun is_closed(): Bool => true
