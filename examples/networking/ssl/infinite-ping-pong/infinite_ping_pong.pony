"""
SSL version of infinite ping-pong.

Same Ping/Pong exchange as the plain infinite-ping-pong example, but over SSL.
Shows both TCPConnection.ssl_server and TCPConnection.ssl_client in the same
program. Both sides use `buffer_until()` so that each `_on_received` callback
delivers exactly one 4-byte message. Must be run from the project root so the
relative certificate paths resolve correctly.
"""
use "constrained_types"
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

    let listen_auth = TCPListenAuth(env.root)
    let connect_auth = TCPConnectAuth(env.root)
    Listener(listen_auth, connect_auth, sslctx, env.out)

actor Listener is TCPListenerActor
  """
  Listens on the example's port, creating an SSL server for each accepted
  connection and starting the SSL client once listening.
  """
  var _tcp_listener: TCPListener = TCPListener.none()
  let _out: OutStream
  let _connect_auth: TCPConnectAuth
  let _server_auth: TCPServerAuth
  let _sslctx: SSLContext val

  new create(listen_auth: TCPListenAuth,
    connect_auth: TCPConnectAuth,
    sslctx: SSLContext val,
    out: OutStream)
  =>
    _connect_auth = connect_auth
    _out = out
    _server_auth = TCPServerAuth(listen_auth)
    _sslctx = sslctx
    _tcp_listener = TCPListener(listen_auth, "127.0.0.1", "7684", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): Server =>
    Server(_server_auth, _sslctx, fd, _out)

  fun ref _on_listening() =>
    Client(_connect_auth, _sslctx, "127.0.0.1", "7684", "", _out)

  fun ref _on_listen_failure() =>
    _out.print("Unable to open listener")

actor Server is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Replies with Pong over SSL for each message it receives.
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
  Sends Ping over SSL on connect and again for each message it receives.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _out: OutStream

  new create(auth: TCPConnectAuth,
    sslctx: SSLContext val,
    host: String,
    port: String,
    from: String,
    out: OutStream)
  =>
    _out = out
    _tcp_connection =
      TCPConnection.ssl_client(
        auth, sslctx, host, port, from, this, this)
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
