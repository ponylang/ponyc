"""
Demonstrates the net package's backpressure handling.

A flood client connects to a sink server and sends 200 chunks of 64KB as fast
as possible. When the OS send buffer fills, send() returns
SendErrorNotWriteable and _on_throttled fires. The client stops sending and
waits for _on_unthrottled to resume. _on_sent confirms each chunk was fully
handed to the OS.

Expected output shows the throttle/unthrottle cycle repeating as the client
outpaces the TCP stack.
"""
use "net"

actor Main
  new create(env: Env) =>
    Listener(TCPListenAuth(env.root), TCPConnectAuth(env.root), env.out)

actor Listener is TCPListenerActor
  """
  Listens on the example's port and starts the flood client once listening.
  """
  var _tcp_listener: TCPListener = TCPListener.none()
  let _out: OutStream
  let _connect_auth: TCPConnectAuth
  let _server_auth: TCPServerAuth

  new create(listen_auth: TCPListenAuth,
    connect_auth: TCPConnectAuth,
    out: OutStream)
  =>
    _connect_auth = connect_auth
    _out = out
    _server_auth = TCPServerAuth(listen_auth)
    _tcp_listener = TCPListener(listen_auth, "127.0.0.1", "7683", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): Sink =>
    Sink(_server_auth, fd, _out)

  fun ref _on_listening() =>
    _out.print("Listener ready, launching flood client...")
    Flood(_connect_auth, "127.0.0.1", "7683", _out)

  fun ref _on_listen_failure() =>
    _out.print("Unable to open listener")

actor Sink is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Server-side connection that receives and discards data.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _out: OutStream
  var _bytes_received: USize = 0

  new create(auth: TCPServerAuth, fd: U32, out: OutStream) =>
    _out = out
    _tcp_connection = TCPConnection.server(auth, fd, this, this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _bytes_received = _bytes_received + data.size()
    KeepReading

  fun ref _on_closed() =>
    _out.print("Sink: received " + _bytes_received.string() + " bytes total")

actor Flood is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Client that sends data as fast as possible, demonstrating how to handle
  backpressure from send().

  The key pattern:
  - Call send() in a loop until all data is sent or SendErrorNotWriteable
  - On SendErrorNotWriteable, stop and wait for _on_unthrottled
  - On _on_unthrottled, resume sending via a deferred behavior
  - Track completion via _on_sent
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _out: OutStream
  let _chunk: Array[U8] val
  let _total_to_send: USize = 200
  var _sends_accepted: USize = 0
  var _sends_confirmed: USize = 0
  var _throttle_count: USize = 0

  new create(auth: TCPConnectAuth,
    host: String,
    port: String,
    out: OutStream)
  =>
    _out = out
    _chunk = recover val Array[U8].init(0x42, 65536) end
    _tcp_connection = TCPConnection.client(auth, host, port, "", this, this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _out.print("Flood: connected, sending " + _total_to_send.string() +
      " chunks of " + _chunk.size().string() + " bytes...")
    // Defer first send batch to a subsequent turn so backpressure goes
    // through the normal ASIO event path.
    _resume_sends()

  fun ref _send_chunks() =>
    """
    Send as many chunks as possible. Stops when all chunks are accepted or
    when backpressure makes the socket unwriteable.
    """
    while _sends_accepted < _total_to_send do
      match \exhaustive\ _tcp_connection.send(_chunk)
      | SendAccepted =>
        // Count after send() returns. _on_sent can fire from inside it, and
        // on the last chunk _on_sent closes the connection. The count has to
        // reach _total_to_send before the loop condition is tested again, or
        // the loop sends on a closed connection.
        _sends_accepted = _sends_accepted + 1
      | SendErrorNotWriteable =>
        // Backpressure active. _on_throttled has already fired.
        // Wait for _on_unthrottled to resume.
        return
      | SendErrorNotConnected =>
        _out.print("Flood: connection lost during send")
        return
      end
    end

  fun ref _on_throttled() =>
    _throttle_count = _throttle_count + 1
    _out.print("Flood: throttled (#" + _throttle_count.string() +
      ") — " + _sends_accepted.string() + "/" + _total_to_send.string() +
      " chunks accepted")

  fun ref _on_unthrottled() =>
    _out.print("Flood: unthrottled, resuming sends")
    _resume_sends()

  be _resume_sends() =>
    """
    Resume sending in a new behavior turn. _on_unthrottled runs inside the
    connection's own event dispatch, ahead of the flush that drains what is
    already queued, so sending straight from it re-enters the connection
    mid-dispatch and puts the whole chunk loop in that same turn.
    """
    _send_chunks()

  fun ref _on_sent(token: SendToken) =>
    _sends_confirmed = _sends_confirmed + 1
    if _sends_confirmed == _total_to_send then
      _out.print("Flood: all " + _total_to_send.string() +
        " sends confirmed by OS. Throttled " +
        _throttle_count.string() + " time(s).")
      _tcp_connection.close()
    end

  fun ref _on_send_failed(token: SendToken) =>
    _out.print("Flood: send failed (connection closed with pending write)")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _out.print("Flood: connection failed")

  fun ref _on_closed() =>
    _out.print("Flood: closed")
