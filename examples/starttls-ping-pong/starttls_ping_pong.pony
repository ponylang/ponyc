"""
STARTTLS upgrade from plaintext to TLS mid-connection.

The client connects over plain TCP, sends "STARTTLS" to request an upgrade, and
the server replies "OK" then upgrades its side. The client upgrades after
receiving "OK". Once both sides complete the TLS handshake, they exchange
Ping/Pong messages over the encrypted connection — the same back-and-forth as
infinite-ping-pong, but starting plaintext and switching to TLS mid-stream.

Shows `start_tls()` on both sides, `_on_tls_ready` for post-handshake
notification, and the STARTTLS negotiation pattern used by protocols like
PostgreSQL and SMTP. Must be run from the project root so the relative
certificate paths resolve correctly.

IMPORTANT: This example treats each `_on_received` callback as delivering a
complete message ("STARTTLS", "OK", "Ping", "Pong"). TCP is stream-oriented, not
message-oriented — in a real program, data can arrive split across multiple
reads or concatenated together. A production implementation needs to account
for this; common approaches include length-prefixed framing (see the
framed-protocol example), line-delimited protocols, or using `buffer_until()`
to read fixed-size chunks. The framing code is omitted here to keep the
STARTTLS flow easy to follow.
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

    let listen_auth = TCPListenAuth(env.root)
    let connect_auth = TCPConnectAuth(env.root)
    Listener(listen_auth, connect_auth, sslctx, env.out)

actor Listener is TCPListenerActor
  """
  Listens on the example's port, creating a server for each accepted connection
  and starting the client once listening.
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
    _tcp_listener = TCPListener(listen_auth, "127.0.0.1", "7671", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): Server =>
    Server(_server_auth, _sslctx, fd, _out)

  fun ref _on_listening() =>
    _out.print("Listener ready, starting client")
    Client(_connect_auth, _sslctx, "127.0.0.1", "7671", "", _out)

  fun ref _on_listen_failure() =>
    _out.print("Unable to open listener")

actor Server is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Replies to a STARTTLS request by upgrading to TLS, then answers each Ping with
  Pong.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _out: OutStream

  new create(auth: TCPServerAuth,
    sslctx: SSLContext val,
    fd: U32,
    out: OutStream)
  =>
    _out = out
    _sslctx = sslctx
    _tcp_connection = TCPConnection.server(auth, fd, this, this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let msg = String.from_array(consume data)
    if msg == "STARTTLS" then
      _out.print("Server: received STARTTLS, sending OK and upgrading")
      _tcp_connection.send("OK")
      match _tcp_connection.start_tls(_sslctx)
      | let err: StartTLSError =>
        _out.print("Server: start_tls failed")
      end
    else
      _out.print("Server: " + msg)
      _tcp_connection.send("Pong")
    end
    KeepReading

  fun ref _on_tls_ready() =>
    _out.print("Server: TLS handshake complete")

  fun ref _on_tls_failure(reason: TLSFailureReason) =>
    _out.print("Server: TLS handshake failed")

actor Client is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Requests a STARTTLS upgrade, upgrades to TLS on OK, then exchanges Ping/Pong
  over the encrypted connection.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _out: OutStream

  new create(auth: TCPConnectAuth,
    sslctx: SSLContext val,
    host: String,
    port: String,
    from: String,
    out: OutStream)
  =>
    _out = out
    _sslctx = sslctx
    _tcp_connection = TCPConnection.client(auth, host, port, from, this, this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _out.print("Client: connected (plaintext), requesting STARTTLS")
    _tcp_connection.send("STARTTLS")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let msg = String.from_array(consume data)
    if msg == "OK" then
      _out.print("Client: received OK, upgrading to TLS")
      match _tcp_connection.start_tls(_sslctx, "127.0.0.1")
      | let err: StartTLSError =>
        _out.print("Client: start_tls failed")
      end
    else
      _out.print("Client: " + msg)
      _tcp_connection.send("Ping")
    end
    KeepReading

  fun ref _on_tls_ready() =>
    _out.print("Client: TLS handshake complete, sending first Ping")
    _tcp_connection.send("Ping")

  fun ref _on_tls_failure(reason: TLSFailureReason) =>
    _out.print("Client: TLS handshake failed")
