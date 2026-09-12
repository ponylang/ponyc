"""
Demonstrates returning `YieldReading` for cooperative scheduler fairness.

A flood client sends 100 four-byte messages as fast as possible. The server
returns `YieldReading` every 10 messages, which exits the read loop and lets
other actors run before reading resumes automatically in the next scheduler
turn. The server prints progress at each yield point.

This pattern is useful when a single connection receives sustained high-volume
traffic and you want to prevent it from monopolizing the Pony scheduler. Unlike
`mute()`/`unmute()`, `YieldReading` is a one-shot pause — reading resumes on
its own without explicit action.
"""
use "constrained_types"
use "net"

actor Main
  new create(env: Env) =>
    Listener(TCPListenAuth(env.root), TCPConnectAuth(env.root), env.out)

actor Listener is TCPListenerActor
  """
  Listens on the example's port and starts the client once listening.
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
    _tcp_listener = TCPListener(listen_auth, "127.0.0.1", "7673", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): Server =>
    Server(_server_auth, fd, _out)

  fun ref _on_listening() =>
    _out.print("Listener ready, launching client...")
    Client(_connect_auth, "127.0.0.1", "7673", _out)

  fun ref _on_listen_failure() =>
    _out.print("Unable to open listener")

actor Server is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Server that yields the read loop every 10 messages to give other actors
  a chance to run.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _out: OutStream
  var _received_count: USize = 0

  new create(auth: TCPServerAuth, fd: U32, out: OutStream) =>
    _out = out
    _tcp_connection = TCPConnection.server(auth, fd, this, this)
    match MakeBufferSize(4)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _received_count = _received_count + 1

    if _received_count == 100 then
      _out.print("Server: all 100 messages received.")
      _tcp_connection.close()
    end

    if (_received_count % 10) == 0 then
      _out.print("Server: received " + _received_count.string() +
        " messages, yielding...")
      return YieldReading
    end

    KeepReading

  fun ref _on_closed() =>
    _out.print("Server: closed")

actor Client is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Client that sends 100 four-byte messages as fast as possible.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _out: OutStream

  new create(auth: TCPConnectAuth,
    host: String,
    port: String,
    out: OutStream)
  =>
    _out = out
    _tcp_connection = TCPConnection.client(auth, host, port, "", this, this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _out.print("Client: connected, sending 100 messages...")
    var i: USize = 0
    while i < 100 do
      _tcp_connection.send("Ping")
      i = i + 1
    end

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _out.print("Client: connection failed")

  fun ref _on_closed() =>
    _out.print("Client: closed")
