"""
Client and server exchanging messages in an endless loop.

A listener starts a server, then launches a client that connects and sends
"Ping". The server prints each message and replies with "Pong". The client
prints each reply and sends "Ping" again, producing an infinite back-and-forth.

Shows both sides of a TCP conversation: ServerLifecycleEventReceiver for the
server and ClientLifecycleEventReceiver for the client. Both sides use
`buffer_until()` so that each `_on_received` callback delivers exactly one
4-byte message ("Ping" or "Pong").
"""
use "constrained_types"
use "net"

actor Main
  new create(env: Env) =>
    let listen_auth = TCPListenAuth(env.root)
    let connect_auth = TCPConnectAuth(env.root)
    Listener(listen_auth, connect_auth, env.out)

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
    _tcp_listener = TCPListener(listen_auth, "127.0.0.1", "7682", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): Server =>
    Server(_server_auth, fd, _out)

  fun ref _on_listening() =>
    Client(_connect_auth, "127.0.0.1", "7682", "", _out)

  fun ref _on_listen_failure() =>
    _out.print("Unable to open listener")

actor Server is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Receives each message and replies with Pong.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _out: OutStream

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
    _out.print(consume data)
    _tcp_connection.send("Pong")
    KeepReading

actor Client is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Sends Ping when connected and sends Ping again for each reply received.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _out: OutStream

  new create(auth: TCPConnectAuth,
    host: String,
    port: String,
    from: String,
    out: OutStream)
  =>
    _out = out
    _tcp_connection = TCPConnection.client(auth, host, port, from, this, this)
    match MakeBufferSize(4)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _tcp_connection.send("Ping")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _out.print(consume data)
    _tcp_connection.send("Ping")
    KeepReading
