"""
IPv4-only echo server with a built-in client.

Demonstrates using the `ip_version` parameter to restrict both the listener and
client to IPv4. The listener binds to `127.0.0.1` with `IP4`, and the client
connects with `IP4`. The same approach works with `IP6` for IPv6-only
connections.

The client sends "Hello, IPv4!" and the server echoes it back. The client prints
the response and closes the connection.
"""
use "net"

actor Main
  new create(env: Env) =>
    let listen_auth = TCPListenAuth(env.root)
    let connect_auth = TCPConnectAuth(env.root)
    IP4EchoListener(listen_auth, connect_auth, env.out)

actor IP4EchoListener is TCPListenerActor
  """
  Listens on the example's port over IPv4, creating an echo server for each
  accepted connection and launching the IPv4 client once listening.
  """
  var _tcp_listener: TCPListener = TCPListener.none()
  let _out: OutStream
  let _server_auth: TCPServerAuth
  let _connect_auth: TCPConnectAuth
  var _client: (IP4EchoClient | None) = None

  new create(listen_auth: TCPListenAuth,
    connect_auth: TCPConnectAuth,
    out: OutStream)
  =>
    _out = out
    _connect_auth = connect_auth
    _server_auth = TCPServerAuth(listen_auth)
    _tcp_listener =
      TCPListener(
        listen_auth, "127.0.0.1", "7674", this
        where ip_version = IP4)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): IP4Echoer =>
    IP4Echoer(_server_auth, fd, _out)

  fun ref _on_closed() =>
    try (_client as IP4EchoClient).dispose() end

  fun ref _on_listen_failure() =>
    _out.print("Couldn't start IPv4 echo server.")

  fun ref _on_listening() =>
    _out.print("IPv4 echo server started on 127.0.0.1:7674")
    _client = IP4EchoClient(_connect_auth, _out)

actor IP4Echoer is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Echoes back over IPv4 whatever it receives.
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

  fun ref _on_closed() =>
    _out.print("Server: connection closed.")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _out.print("Server: echoing data back.")
    _tcp_connection.send(consume data)
    KeepReading

actor IP4EchoClient is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Connects over IPv4, sends a message, prints the echoed reply, and closes.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _out: OutStream

  new create(auth: TCPConnectAuth, out: OutStream) =>
    _out = out
    _tcp_connection =
      TCPConnection.client(
        auth, "127.0.0.1", "7674", "", this, this where ip_version = IP4)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _out.print("Client: connected over IPv4.")
    _tcp_connection.send("Hello, IPv4!")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _out.print("Client: connection failed.")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _out.print("Client: received '" + String.from_array(consume data) + "'")
    _tcp_connection.close()
    KeepReading
