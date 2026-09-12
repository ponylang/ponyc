"""
Demonstrates per-send completion tracking.

Every accepted send() delivers a SendToken to _on_send_accepted, and that
token gets exactly one terminal callback: _on_sent(token) once its bytes reach
the OS, or _on_send_failed(token) if the connection closes first. The token
identifies WHICH send completed, so an application can say which of its sends
have been handed to the OS -- not just how many.

This client sends five labeled messages up front and keeps a map of the ones
still outstanding, keyed by token id. As each _on_sent arrives it reports which
message completed and drops it from the map; once every message has been
issued and the map is empty, every send has reached the OS and the client
closes. If the connection had dropped with sends still outstanding,
_on_send_failed would report which ones did not make it -- the same map, read
the other way.

Over loopback with nothing in the way, each send reaches the OS inside the
send() call that queued it, so the printed output shows each message recorded
and completed in turn and the map holding one entry at a time. When the peer
is slower than the sender the completions lag behind the sends and the map
holds several at once. The bookkeeping is the same either way.

Because _on_sent can fire from inside send(), an empty map on its own does not
mean the client is finished -- it has to have issued every message as well.

"Reached the OS" means written to the kernel send buffer, not received by the
peer. End-to-end delivery is still the application's job.
"""
use "net"
use "collections"

actor Main
  new create(env: Env) =>
    Listener(TCPListenAuth(env.root), TCPConnectAuth(env.root), env.out)

actor Listener is TCPListenerActor
  """
  Listens on the example's port and starts the sender once listening.
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
    _tcp_listener = TCPListener(listen_auth, "127.0.0.1", "7688", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): Sink =>
    Sink(_server_auth, fd, _out)

  fun ref _on_listening() =>
    _out.print("Listener ready, launching client...")
    Sender(_connect_auth, "127.0.0.1", "7688", _out)

  fun ref _on_listen_failure() =>
    _out.print("Unable to open listener")

actor Sink is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Server-side connection that receives and discards data.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _out: OutStream

  new create(auth: TCPServerAuth, fd: U32, out: OutStream) =>
    _out = out
    _tcp_connection = TCPConnection.server(auth, fd, this, this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    KeepReading

  fun ref _on_closed() =>
    _out.print("Sink: connection closed")

actor Sender is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Sends several labeled messages and tracks which ones have reached the OS by
  their SendToken.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _out: OutStream
  let _messages: Array[String] val
  // Sends still waiting for _on_sent, keyed by token id.
  let _outstanding: Map[USize, String] = _outstanding.create()
  var _all_issued: Bool = false

  new create(auth: TCPConnectAuth,
    host: String,
    port: String,
    out: OutStream)
  =>
    _out = out
    _messages =
      recover val
        ["login"; "subscribe"; "query"; "heartbeat"; "logout"]
      end
    _tcp_connection = TCPConnection.client(auth, host, port, "", this, this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _out.print("Sender: connected, sending " + _messages.size().string() +
      " messages")
    for msg in _messages.values() do
      match \exhaustive\ _tcp_connection.send(msg)
      | SendAccepted => None
      | let _: SendError =>
        _out.print("  could not send '" + msg + "'")
      end
    end
    _all_issued = true
    _maybe_close()

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    // `data` is what was handed to send(), so there is nothing to stash before
    // the call to have the message here.
    let msg =
      match data
      | let s: String => s
      else
        // This example only ever sends a String.
        ""
      end

    _outstanding(token.id) = msg
    _out.print("  sent '" + msg + "' (token " + token.id.string() +
      "), awaiting _on_sent")

  fun ref _on_sent(token: SendToken) =>
    try
      (_, let msg) = _outstanding.remove(token.id)?
      _out.print("_on_sent: '" + msg + "' (token " + token.id.string() +
        ") reached the OS; " + _outstanding.size().string() +
        " still outstanding")
    end
    _maybe_close()

  fun ref _maybe_close() =>
    """
    Close once every message has been handed to send() and every one of those
    sends has reached the OS. An empty outstanding map alone is not enough:
    _on_sent fires from inside send(), so the map empties again between
    messages while there are still messages left to issue.
    """
    if _all_issued and (_outstanding.size() == 0) then
      _out.print("Sender: every send reached the OS, closing")
      _tcp_connection.close()
    end

  fun ref _on_send_failed(token: SendToken) =>
    try
      (_, let msg) = _outstanding.remove(token.id)?
      _out.print("_on_send_failed: '" + msg + "' (token " + token.id.string() +
        ") did not reach the OS before the connection closed")
    end

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _out.print("Sender: connection failed")

  fun ref _on_closed() =>
    _out.print("Sender: closed")
