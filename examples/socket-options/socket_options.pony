"""
Socket option tuning on a connected TCP connection.

A self-contained echo server that configures TCP_NODELAY and OS buffer sizes on
each accepted connection using both dedicated convenience methods and the
general-purpose `getsockopt`/`setsockopt` interface. The client connects, sends
"Hello", receives the echo, and prints the configured socket option values
before closing. Demonstrates `set_nodelay()`, `set_so_rcvbuf()`,
`get_so_rcvbuf()`, `setsockopt_u32()`, and `getsockopt_u32()` on a live
connection.
"""
use "net"

actor Main
  new create(env: Env) =>
    let listen_auth = TCPListenAuth(env.root)
    let connect_auth = TCPConnectAuth(env.root)
    SocketOptionsListener(
      listen_auth, connect_auth, "127.0.0.1", "7676", env.out)

actor SocketOptionsListener is TCPListenerActor
  """
  Listens on the example's port, creating an echo server for each accepted
  connection and starting the client once listening.
  """
  var _tcp_listener: TCPListener = TCPListener.none()
  let _out: OutStream
  let _connect_auth: TCPConnectAuth
  let _server_auth: TCPServerAuth

  new create(listen_auth: TCPListenAuth,
    connect_auth: TCPConnectAuth,
    host: String,
    port: String,
    out: OutStream)
  =>
    _out = out
    _connect_auth = connect_auth
    _server_auth = TCPServerAuth(listen_auth)
    _tcp_listener = TCPListener(listen_auth, host, port, this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): SocketOptionsServer =>
    SocketOptionsServer(_server_auth, fd, _out)

  fun ref _on_listening() =>
    _out.print("Listener started on 127.0.0.1:7676")
    SocketOptionsClient(_connect_auth, "127.0.0.1", "7676", _out)

  fun ref _on_listen_failure() =>
    _out.print("Couldn't start listener.")

  fun ref _on_closed() =>
    _out.print("Listener closed.")

actor SocketOptionsServer is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Configures TCP_NODELAY and OS buffer sizes on each accepted connection, then
  echoes back what it receives.
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

  fun ref _on_started() =>
    // Disable Nagle for low-latency responses
    _tcp_connection.set_nodelay(true)

    // Set OS buffer sizes using convenience methods
    _tcp_connection.set_so_rcvbuf(32768)

    // Set send buffer using the general-purpose setsockopt_u32 interface.
    // Any option from OSSockOpt can be set this way.
    _tcp_connection.setsockopt_u32(
      OSSockOpt.sol_socket(), OSSockOpt.so_sndbuf(), 32768)

    // Read back the actual values (OS may round up).
    // Convenience method for receive buffer:
    (let rcv_errno: U32, let rcv_size: U32) =
      _tcp_connection.get_so_rcvbuf()
    // General-purpose method for send buffer:
    (let snd_errno: U32, let snd_size: U32) =
      _tcp_connection.getsockopt_u32(
        OSSockOpt.sol_socket(), OSSockOpt.so_sndbuf())

    if (rcv_errno == 0) and (snd_errno == 0) then
      _out.print("Server: rcvbuf=" + rcv_size.string() +
        " sndbuf=" + snd_size.string())
    else
      _out.print("Server: failed to read buffer sizes")
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _tcp_connection.send(consume data)
    KeepReading

  fun ref _on_closed() =>
    _out.print("Server: connection closed")

actor SocketOptionsClient is
  (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Connects, sends a message, prints the echoed reply, and closes.
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

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _tcp_connection.send("Hello")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _out.print("Client: received echo: " + String.from_array(consume data))
    _tcp_connection.close()
    KeepReading

  fun ref _on_closed() =>
    _out.print("Client: closed")
