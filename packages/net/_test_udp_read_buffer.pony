use "pony_test"
use "constrained_types"

primitive \nodoc\ _AscendingBytes
  """
  An array of `n` bytes where byte `i` holds `i.u8()`, so for `n <= 256`
  every byte is distinct. A wrong-offset, reordered, or stale-memory delivery
  fails on content, not just length.
  """
  fun apply(n: USize): Array[U8] val =>
    recover val
      let a = Array[U8](n)
      var i: USize = 0
      while i < n do
        a.push(i.u8())
        i = i + 1
      end
      a
    end

actor \nodoc\ _TestUDPReadBufferReceiver
  is (UDPSocketActor & UDPLifecycleEventReceiver)
  """
  Receives one datagram and asserts its content matches `_expected`.
  On bind, spawns a sender aimed at this socket's ephemeral port.
  """
  var _udp: UDPSocket = UDPSocket.none()
  let _h: TestHelper
  let _expected: Array[U8] val
  let _payload: Array[U8] val
  var _sender: (_TestUDPReadBufferSender | None) = None

  new create(auth: UDPAuth,
    host: String,
    port: String,
    h: TestHelper,
    expected: Array[U8] val,
    payload: Array[U8] val,
    read_buffer_size: ReadBufferSize)
  =>
    _h = h
    _expected = expected
    _payload = payload
    _udp =
      UDPSocket(
        auth,
        host,
        port,
        this,
        this
        where read_buffer_size = read_buffer_size, ip_version = IP4)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    let addr = _udp.local_address()
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    _sender =
      _TestUDPReadBufferSender(UDPAuth(_h.env.root), host, addr, _h, _payload)

  fun ref _on_bind_failure() =>
    _h.fail("receiver bind failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    let got: Array[U8] = consume data
    _h.assert_eq[USize](got.size(), _expected.size())
    _h.assert_array_eq[U8](_expected, got)
    _udp.close()
    KeepReading

  fun ref _on_closed() =>
    match _sender
    | let s: _TestUDPReadBufferSender => s.dispose()
    end
    _h.complete(true)

actor \nodoc\ _TestUDPReadBufferSender
  is (UDPSocketActor & UDPLifecycleEventReceiver)
  """
  Sends `_payload` to `_dest` on bind, then retransmits via self-behavior
  until the socket closes.
  """
  var _udp: UDPSocket = UDPSocket.none()
  let _h: TestHelper
  let _dest: NetAddress val
  let _payload: Array[U8] val
  var _attempts: U32 = 0

  new create(auth: UDPAuth,
    host: String,
    dest: NetAddress val,
    h: TestHelper,
    payload: Array[U8] val)
  =>
    _h = h
    _dest = dest
    _payload = payload
    _udp = UDPSocket(auth, host, "0", this, this where ip_version = IP4)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    _udp.send_to(_payload, _dest)
    _retransmit()

  fun ref _on_bind_failure() =>
    _h.fail("sender bind failed")
    _h.complete(false)

  be _retransmit() =>
    if _udp.is_open() then
      _attempts = _attempts + 1
      if _attempts > 100 then
        _udp.close()
      else
        _udp.send_to(_payload, _dest)
        _retransmit()
      end
    end

class \nodoc\ iso _TestUDPOversizedDatagramTruncated is UnitTest
  """
  A datagram larger than the receiver's read buffer is delivered truncated to
  exactly the buffer size, holding the payload's first `size` bytes; the
  excess is silently discarded. Buffer 64, payload 200: delivered is the
  first 64 bytes.
  """
  fun name(): String => "net/UDPOversizedDatagramTruncated"

  fun ref apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let rbs =
      match \exhaustive\ MakeReadBufferSize(64)
      | let r: ReadBufferSize => r
      | let _: ValidationFailure =>
        _Unreachable()
        return
      end
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    let receiver =
      _TestUDPReadBufferReceiver(
        UDPAuth(h.env.root),
        host,
        "9820",
        h,
        _AscendingBytes(64),
        _AscendingBytes(200),
        rbs)
    h.dispose_when_done(receiver)

class \nodoc\ iso _TestUDPUndersizedDatagramDelivered is UnitTest
  """
  A datagram smaller than the receiver's read buffer is delivered whole, with
  no trailing buffer bytes. Buffer 64, payload 20: delivered is all 20 bytes.
  """
  fun name(): String => "net/UDPUndersizedDatagramDelivered"

  fun ref apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let rbs =
      match \exhaustive\ MakeReadBufferSize(64)
      | let r: ReadBufferSize => r
      | let _: ValidationFailure =>
        _Unreachable()
        return
      end
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    let receiver =
      _TestUDPReadBufferReceiver(
        UDPAuth(h.env.root),
        host,
        "9821",
        h,
        _AscendingBytes(20),
        _AscendingBytes(20),
        rbs)
    h.dispose_when_done(receiver)

class \nodoc\ iso _TestUDPEmptyDatagramDelivered is UnitTest
  """
  A zero-byte UDP datagram (valid per RFC 768) is delivered with an empty
  payload, not treated as an error. Pins the connectionless `recvfrom`
  contract that a 0-byte read means empty datagram, not peer close.
  """
  fun name(): String => "net/UDPEmptyDatagramDelivered"

  fun ref apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let rbs =
      match \exhaustive\ MakeReadBufferSize(64)
      | let r: ReadBufferSize => r
      | let _: ValidationFailure =>
        _Unreachable()
        return
      end
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    let receiver =
      _TestUDPReadBufferReceiver(
        UDPAuth(h.env.root),
        host,
        "9822",
        h,
        _AscendingBytes(0),
        _AscendingBytes(0),
        rbs)
    h.dispose_when_done(receiver)

class \nodoc\ iso _TestUDPSmallReadBufferTruncates is UnitTest
  """
  A socket with a 1-byte read buffer delivers only the first byte of a
  larger datagram. Pins the minimum-buffer behavior: `ReadBufferSize`
  rejects 0, so the smallest valid buffer is 1. A 3-byte payload is
  delivered as its first byte only.
  """
  fun name(): String => "net/UDPSmallReadBufferTruncates"

  fun ref apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let rbs =
      match \exhaustive\ MakeReadBufferSize(1)
      | let r: ReadBufferSize => r
      | let _: ValidationFailure =>
        _Unreachable()
        return
      end
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    let receiver =
      _TestUDPReadBufferReceiver(
        UDPAuth(h.env.root),
        host,
        "9823",
        h,
        _AscendingBytes(1),
        _AscendingBytes(3),
        rbs)
    h.dispose_when_done(receiver)
