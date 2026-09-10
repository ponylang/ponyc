"""
SSL version of the echo server.

Follows the same structure as the plain echo server, adding SSLContext setup
and using TCPConnection.ssl_server instead of TCPConnection.server. The SSL
handshake happens transparently inside TCPConnection.

Must be run from the project root so the relative certificate paths resolve
correctly. Connect with an SSL client (e.g. `openssl s_client -connect
localhost:7681`) to test.
"""
use "files"
use "net"

actor Main
  new create(env: Env) =>
    let file_auth = FileAuth(env.root)
    let sslctx =
      try
        // paths need to be adjusted to a absolute location or you need to run
        // the example from a location where this relative path will be valid
        // aka the root of this project
        recover val
          SSLContext
            .> set_authority(
              FilePath(file_auth, "assets/cert.pem"))?
            .> set_cert(
              FilePath(file_auth, "assets/cert.pem"),
              FilePath(file_auth, "assets/key.pem"))?
            .> set_client_verify(false)
            .> set_server_verify(false)
        end
      else
        env.out.print("unable to set up SSL authentication")
        return
      end

    let auth = TCPListenAuth(env.root)
    let echo = EchoServer(auth, sslctx, "", "7681", env.out)

actor EchoServer is TCPListenerActor
  """
  Listens on the example's port and creates an SSL echoer for each accepted
  connection.
  """
  var _tcp_listener: TCPListener = TCPListener.none()
  let _out: OutStream
  let _server_auth: TCPServerAuth
  let _sslctx: SSLContext val

  new create(listen_auth: TCPListenAuth,
    sslctx: SSLContext val,
    host: String,
    port: String,
    out: OutStream)
  =>
    _out = out
    _sslctx = sslctx
    _server_auth = TCPServerAuth(listen_auth)
    _tcp_listener = TCPListener(listen_auth, host, port, this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): Echoer =>
    Echoer(_server_auth, _sslctx, fd, _out)

  fun ref _on_closed() =>
    _out.print("Echo server shut down.")

  fun ref _on_listen_failure() =>
    _out.print("Couldn't start Echo server. " +
      "Perhaps try another network interface?")

  fun ref _on_listening() =>
    _out.print("Echo server started.")

actor Echoer is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Echoes back over SSL whatever it receives.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _out: OutStream

  new create(auth: TCPServerAuth,
    sslctx: SSLContext val,
    fd: U32,
    out: OutStream)
  =>
    _out = out
    _tcp_connection = TCPConnection.ssl_server(auth, sslctx, fd, this, this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_closed() =>
    _out.print("Connection Closed")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _out.print("Decrypted Data received. Echoing it back.")
    _connection().send(consume data)
    KeepReading
