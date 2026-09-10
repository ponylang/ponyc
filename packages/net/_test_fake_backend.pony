use "pony_test"

// FFI: raw socket — declaration lives in _test_stale_foreign_event.pony

// ---------------------------------------------------------------------------
// Shared stub methods
// ---------------------------------------------------------------------------
//
// Every fake backend is a class implementing TCPBackend. Most methods are
// identical stubs — only sendv, receive, connect, listen, or accept differ.
// Pony classes cannot inherit, so the stubs are repeated in each class.
// The per-class docstring says which methods carry test behaviour; the rest
// are stubs.

// ---------------------------------------------------------------------------
// Server-side fake backends
// ---------------------------------------------------------------------------
// Used with TCPConnection[X].server(). The server path never calls connect,
// listen, or accept, so those are stubs. close releases the raw fd the test
// allocated.

class \nodoc\ _FBSendOkRecvRetry is TCPBackend
  """
  sendv: accepts all bytes.  receive: always retries.
  """
  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref accept(event: AsioEventID): I32 => 0

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    Array[AsioEventID]

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize) ?
  =>
    var total: USize = 0
    var i = from
    let stop = from + count
    while i < stop do
      let s = data(i)?.size()
      total = total + if i == from then s - first_buffer_byte_offset else s end
      i = i + 1
    end
    (SocketResultOk, total)

class \nodoc\ _FBSendErrorRecvRetry is TCPBackend
  """
  sendv: always returns error.  receive: always retries.
  """
  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref accept(event: AsioEventID): I32 => 0

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    Array[AsioEventID]

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize)
  =>
    (SocketResultError, 0)

class \nodoc\ _FBSendStepRecvRetryFailed is TCPBackend
  """
  sendv: step 0 returns retry, step 1+ returns ok (all bytes).
  receive: always retries.
  """
  var _step: USize = 0

  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref accept(event: AsioEventID): I32 => 0

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    Array[AsioEventID]

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize) ?
  =>
    let step = _step
    _step = _step + 1
    if step == 0 then
      (SocketResultRetry, 0)
    else
      var total: USize = 0
      var i = from
      let stop = from + count
      while i < stop do
        let s = data(i)?.size()
        total =
          total + if i == from then s - first_buffer_byte_offset else s end
        i = i + 1
      end
      (SocketResultOk, total)
    end

class \nodoc\ _FBSendOkRecvHello is TCPBackend
  """
  sendv: accepts all bytes.  receive: step 0 copies "hello" into buffer,
  step 1+ returns retry.
  """
  var _step: USize = 0

  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref accept(event: AsioEventID): I32 => 0

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    Array[AsioEventID]

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    let step = _step
    _step = _step + 1
    if step == 0 then
      let src: Array[U8] val = [as U8: 'h'; 'e'; 'l'; 'l'; 'o']
      @memcpy(buffer, src.cpointer(), 5)
      (SocketResultOk, 5)
    else
      (SocketResultRetry, 0)
    end

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize) ?
  =>
    var total: USize = 0
    var i = from
    let stop = from + count
    while i < stop do
      let s = data(i)?.size()
      total = total + if i == from then s - first_buffer_byte_offset else s end
      i = i + 1
    end
    (SocketResultOk, total)

class \nodoc\ _FBSendOkRecv10 is TCPBackend
  """
  sendv: accepts all bytes.  receive: step 0 copies 10 bytes ('A' repeated)
  into buffer, step 1+ returns retry.
  """
  var _step: USize = 0

  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref accept(event: AsioEventID): I32 => 0

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    Array[AsioEventID]

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    let step = _step
    _step = _step + 1
    if step == 0 then
      let src: Array[U8] val = recover val Array[U8].init('A', 10) end
      @memcpy(buffer, src.cpointer(), 10)
      (SocketResultOk, 10)
    else
      (SocketResultRetry, 0)
    end

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize) ?
  =>
    var total: USize = 0
    var i = from
    let stop = from + count
    while i < stop do
      let s = data(i)?.size()
      total = total + if i == from then s - first_buffer_byte_offset else s end
      i = i + 1
    end
    (SocketResultOk, total)

class \nodoc\ _FBSendOkRecvHelloMute is TCPBackend
  """
  sendv: accepts all bytes.  receive: step 0 copies "hello" into buffer,
  step 1+ returns retry.
  """
  var _step: USize = 0

  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref accept(event: AsioEventID): I32 => 0

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    Array[AsioEventID]

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    let step = _step
    _step = _step + 1
    if step == 0 then
      let src: Array[U8] val = [as U8: 'h'; 'e'; 'l'; 'l'; 'o']
      @memcpy(buffer, src.cpointer(), 5)
      (SocketResultOk, 5)
    else
      (SocketResultRetry, 0)
    end

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize) ?
  =>
    var total: USize = 0
    var i = from
    let stop = from + count
    while i < stop do
      let s = data(i)?.size()
      total = total + if i == from then s - first_buffer_byte_offset else s end
      i = i + 1
    end
    (SocketResultOk, total)

class \nodoc\ _FBSendOkRecvFillBuffer is TCPBackend
  """
  sendv: accepts all bytes.  receive: step 0 copies 16 bytes ('A' repeated)
  into buffer, step 1+ returns retry. Designed for a 16-byte read buffer with
  buffer_until(4): after one 4-byte frame is chopped off, the remaining 12
  bytes fill the buffer completely.
  """
  var _step: USize = 0

  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref accept(event: AsioEventID): I32 => 0

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    Array[AsioEventID]

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    let step = _step
    _step = _step + 1
    if step == 0 then
      let src: Array[U8] val = recover val Array[U8].init('A', 16) end
      @memcpy(buffer, src.cpointer(), 16)
      (SocketResultOk, 16)
    else
      (SocketResultRetry, 0)
    end

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize) ?
  =>
    var total: USize = 0
    var i = from
    let stop = from + count
    while i < stop do
      let s = data(i)?.size()
      total = total + if i == from then s - first_buffer_byte_offset else s end
      i = i + 1
    end
    (SocketResultOk, total)

class \nodoc\ _FBSendOkRecv10Yield is TCPBackend
  """
  sendv: accepts all bytes.  receive: step 0 copies 10 bytes ('A' repeated)
  into buffer, step 1+ returns retry.
  """
  var _step: USize = 0

  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref accept(event: AsioEventID): I32 => 0

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    Array[AsioEventID]

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    let step = _step
    _step = _step + 1
    if step == 0 then
      let src: Array[U8] val = recover val Array[U8].init('A', 10) end
      @memcpy(buffer, src.cpointer(), 10)
      (SocketResultOk, 10)
    else
      (SocketResultRetry, 0)
    end

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize) ?
  =>
    var total: USize = 0
    var i = from
    let stop = from + count
    while i < stop do
      let s = data(i)?.size()
      total = total + if i == from then s - first_buffer_byte_offset else s end
      i = i + 1
    end
    (SocketResultOk, total)

class \nodoc\ _FBSendOkRecvError is TCPBackend
  """
  sendv: accepts all bytes.  receive: always returns error.
  """
  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref accept(event: AsioEventID): I32 => 0

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    Array[AsioEventID]

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    (SocketResultError, 0)

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize) ?
  =>
    var total: USize = 0
    var i = from
    let stop = from + count
    while i < stop do
      let s = data(i)?.size()
      total = total + if i == from then s - first_buffer_byte_offset else s end
      i = i + 1
    end
    (SocketResultOk, total)

// ---------------------------------------------------------------------------
// Client-side fake backends
// ---------------------------------------------------------------------------
// Used with TCPConnection[X].client(). The client path calls connect; the
// connection never reaches _Open, so sendv/receive are stubs.
class \nodoc\ _FBConnect0 is TCPBackend
  """
  connect: returns an empty array (all attempts failed — DNS failure path).
  """
  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref accept(event: AsioEventID): I32 => 0

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    Array[AsioEventID]

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

class \nodoc\ _FBConnect1 is TCPBackend
  """
  connect: returns one event (one inflight attempt).
  """
  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref accept(event: AsioEventID): I32 => 0

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    let events = Array[AsioEventID](1)
    let fd = @socket(I32(2), I32(1), I32(0))
    if fd >= 0 then
      events.push(PonyAsio.create_event(the_actor, fd.u32()))
    end
    events

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

class \nodoc\ _FBConnect2 is TCPBackend
  """
  connect: returns two events (two inflight attempts).
  """
  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref accept(event: AsioEventID): I32 => 0

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    let events = Array[AsioEventID](2)
    var i: USize = 0
    while i < 2 do
      let fd = @socket(I32(2), I32(1), I32(0))
      if fd >= 0 then
        events.push(PonyAsio.create_event(the_actor, fd.u32()))
      end
      i = i + 1
    end
    events

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

class \nodoc\ _FBConnect3 is TCPBackend
  """
  connect: returns three events (three inflight attempts).
  """
  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref accept(event: AsioEventID): I32 => 0

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    let events = Array[AsioEventID](3)
    var i: USize = 0
    while i < 3 do
      let fd = @socket(I32(2), I32(1), I32(0))
      if fd >= 0 then
        events.push(PonyAsio.create_event(the_actor, fd.u32()))
      end
      i = i + 1
    end
    events

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

// ---------------------------------------------------------------------------
// Listener fake backends
// ---------------------------------------------------------------------------
class \nodoc\ _FBListenFail is TCPBackend
  """
  listen: returns a null event (failure path).
  """
  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    AsioEvent.none()

  fun ref accept(event: AsioEventID): I32 => 0

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    Array[AsioEventID]

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

class \nodoc\ _FBListenOk is TCPBackend
  """
  listen: creates a valid ASIO event from a raw socket.
  accept: step 0 returns a raw socket fd, step 1+ returns 0 (would-block).
  """
  var _step: USize = 0

  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    let fd = @socket(I32(2), I32(1), I32(0))
    if fd < 0 then return AsioEvent.none() end
    PonyAsio.create_event(the_actor, fd.u32())

  fun ref accept(event: AsioEventID): I32 =>
    let step = _step
    _step = _step + 1
    if step == 0 then
      @socket(I32(2), I32(1), I32(0))
    else
      0
    end

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    Array[AsioEventID]

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

class \nodoc\ _FBListenOkMulti is TCPBackend
  """
  listen: creates a valid ASIO event from a raw socket.
  accept: always returns a fresh raw socket fd (never returns 0).
  """
  new create() => None

  fun ref listen(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion)
    : AsioEventID
  =>
    let fd = @socket(I32(2), I32(1), I32(0))
    if fd < 0 then return AsioEvent.none() end
    PonyAsio.create_event(the_actor, fd.u32())

  fun ref accept(event: AsioEventID): I32 =>
    @socket(I32(2), I32(1), I32(0))

  fun ref close(fd: U32) => @pony_os_socket_close(fd)

  fun ref connect(the_actor: AsioEventNotify,
    host: String,
    port: String,
    from: String,
    asio_flags: U32,
    ip_version: IPVersion)
    : Array[AsioEventID]
  =>
    Array[AsioEventID]

  fun ref keepalive(fd: U32, secs: U32) => None

  fun ref peername(fd: U32, ip: NetAddress tag): Bool => false

  fun ref shutdown(fd: U32) => None

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool => false

  fun ref writev_max(): I32 => 1024

  fun ref receive(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

  fun ref sendv(event: AsioEventID,
    data: Array[ByteSeq] box,
    from: USize,
    count: USize,
    first_buffer_byte_offset: USize)
    : (SocketResult, USize)
  =>
    (SocketResultRetry, 0)

// ---------------------------------------------------------------------------
// Helper: create a raw socket fd for server-side fake connections
// ---------------------------------------------------------------------------
primitive \nodoc\ _FakeServerFd
  """
  Allocate a raw TCP socket fd suitable for PonyAsio.create_event. The fd is
  never bound or connected — it exists only so ASIO has a valid file
  descriptor. The fake backend handles all I/O.
  """
  fun apply(): U32 ? =>
    let fd = @socket(I32(2), I32(1), I32(0))
    if fd < 0 then error end
    fd.u32()
