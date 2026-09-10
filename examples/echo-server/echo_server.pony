"""
Minimal echo server demonstrating the net package's core pattern.

A listener accepts TCP connections and hands each one to an Echoer actor that
sends received data back to the client. Shows the three building blocks of a
net server: TCPListenerActor for accepting connections, TCPConnectionActor for
event plumbing, and ServerLifecycleEventReceiver for application callbacks.

Connect with any TCP client (e.g. `netcat localhost 7669`) and type to see
your input echoed back.
"""
use "net"

actor Main
  new create(env: Env) =>
    let auth = TCPListenAuth(env.root)
    let echo = EchoServer(auth, "", "7669", env.out)

actor EchoServer is TCPListenerActor
  """
  Listens on the example's port and hands each accepted connection to an echoer.
  """
  var _tcp_listener: TCPListener = TCPListener.none()
  let _out: OutStream
  let _server_auth: TCPServerAuth

  new create(listen_auth: TCPListenAuth,
    host: String,
    port: String,
    out: OutStream)
  =>
    _out = out
    _server_auth = TCPServerAuth(listen_auth)
    _tcp_listener = TCPListener(listen_auth, host, port, this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): Echoer =>
    Echoer(_server_auth, fd, _out)

  fun ref _on_closed() =>
    _out.print("Echo server shut down.")

  fun ref _on_listen_failure() =>
    _out.print("Couldn't start Echo server. " +
      "Perhaps try another network interface?")

  fun ref _on_listening() =>
    _out.print("Echo server started.")

actor Echoer is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Echoes received data back to the client.
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
    _out.print("Connection Closed")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _out.print("Data received. Echoing it back.")
    _tcp_connection.send(consume data)
    KeepReading
