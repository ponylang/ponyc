"""
Echo server that closes idle connections after 10 seconds.

Demonstrates `idle_timeout()` for setting a per-connection inactivity timer
and `_on_idle_timeout()` for handling the expiration. The idle timer uses a
per-connection ASIO timer event — no extra actors or shared `Timers` needed.

Start the server and connect with a TCP client (e.g. `netcat localhost 7672`).
Type to see input echoed back. Stop typing for 10 seconds and the server
closes the connection.
"""
use "net"

actor Main
  new create(env: Env) =>
    IdleTimeoutServer(TCPListenAuth(env.root), "", "7672", env.out)

actor IdleTimeoutServer is TCPListenerActor
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

  fun ref _on_accept(fd: U32): IdleTimeoutEchoer =>
    IdleTimeoutEchoer(_server_auth, fd, _out)

  fun ref _on_closed() =>
    _out.print("Server shut down.")

  fun ref _on_listen_failure() =>
    _out.print("Couldn't start server. Perhaps try another port?")

  fun ref _on_listening() =>
    _out.print("Idle-timeout echo server started on port 7672.")

actor IdleTimeoutEchoer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Echoes received data back and closes the connection after 10 seconds of
  inactivity.
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
    _out.print("Connection established. 10-second idle timeout active.")
    match MakeIdleTimeout(10_000)
    | let t: IdleTimeout =>
      _tcp_connection.idle_timeout(t)
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _out.print("Data received. Echoing it back.")
    _tcp_connection.send(consume data)
    KeepReading

  fun ref _on_idle_timeout() =>
    _out.print("Connection idle for 10 seconds. Closing.")
    _tcp_connection.close()

  fun ref _on_idle_timer_failure() =>
    _out.print("Idle timer subscription failed. Idle detection disabled.")

  fun ref _on_closed() =>
    _out.print("Connection closed.")
