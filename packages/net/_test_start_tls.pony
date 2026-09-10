use "constrained_types"
use "files"
use "pony_test"

class \nodoc\ iso _TestStartTLSPingPong is UnitTest
  """
  Test STARTTLS: connect plain, exchange negotiation, upgrade to TLS, then
  send encrypted data. Client sends "STARTTLS", server replies "OK" and
  upgrades, client upgrades, then client sends "Ping" over TLS and server
  echoes "Pong".
  """
  fun name(): String => "net/StartTLSPingPong"

  fun apply(h: TestHelper) ? =>
    let port = "9733"
    let file_auth = FileAuth(h.env.root)
    let sslctx =
      recover
        SSLContext
          .> set_authority(
            FilePath(file_auth, "assets/cert.pem"))?
          .> set_cert(
            FilePath(file_auth, "assets/cert.pem"),
            FilePath(file_auth, "assets/key.pem"))?
          .> set_client_verify(false)
          .> set_server_verify(false)
      end

    h.expect_action("client tls ready")
    h.expect_action("server tls ready")
    h.expect_action("client got pong")

    let listener =
      _TestStartTLSListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestStartTLSClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _h: TestHelper

  new create(port: String,
    sslctx: SSLContext val,
    h: TestHelper)
  =>
    _sslctx = sslctx
    _h = h

    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
        "localhost",
        port,
        "",
        this,
        this)
    match MakeBufferSize(2)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _tcp_connection.send("STARTTLS")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let msg = String.from_array(consume data)
    if msg == "OK" then
      match _tcp_connection.start_tls(_sslctx, "localhost")
      | let _: StartTLSError =>
        _h.fail("Client start_tls failed")
        _h.complete(false)
      end
    elseif msg == "Pong" then
      _h.complete_action("client got pong")
    else
      _h.fail("Client got unexpected: " + msg)
    end
    KeepReading

  fun ref _on_tls_ready() =>
    _h.complete_action("client tls ready")
    match MakeBufferSize(4)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end
    _tcp_connection.send("Ping")

  fun ref _on_tls_failure(reason: TLSFailureReason) =>
    _h.fail("Client TLS handshake failed")

actor \nodoc\ _TestStartTLSServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _h: TestHelper

  new create(sslctx: SSLContext val,
    fd: U32,
    h: TestHelper)
  =>
    _sslctx = sslctx
    _h = h

    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(_h.env.root),
        fd,
        this,
        this)
    match MakeBufferSize(8)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let msg = String.from_array(consume data)
    if msg == "STARTTLS" then
      _tcp_connection.send("OK")
      match _tcp_connection.start_tls(_sslctx)
      | let _: StartTLSError =>
        _h.fail("Server start_tls failed")
        _h.complete(false)
      end
    elseif msg == "Ping" then
      _tcp_connection.send("Pong")
    else
      _h.fail("Server got unexpected: " + msg)
    end
    KeepReading

  fun ref _on_tls_ready() =>
    _h.complete_action("server tls ready")
    match MakeBufferSize(4)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _on_tls_failure(reason: TLSFailureReason) =>
    _h.fail("Server TLS handshake failed")

actor \nodoc\ _TestStartTLSListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestStartTLSClient | None) = None
  let _servers: Array[_TestStartTLSServer] = Array[_TestStartTLSServer]

  new create(port: String,
    sslctx: SSLContext val,
    h: TestHelper)
  =>
    _port = port
    _sslctx = sslctx
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        _port,
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestStartTLSServer =>
    let server = _TestStartTLSServer(_sslctx, fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try (_client as _TestStartTLSClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    _client =
      _TestStartTLSClient(
        _port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestStartTLSListener")

class \nodoc\ iso _TestStartTLSPreconditions is UnitTest
  """
  Test that start_tls() returns the correct error for each precondition
  violation: not connected, already TLS, and not ready (muted).
  """
  fun name(): String => "net/StartTLSPreconditions"

  fun apply(h: TestHelper) ? =>
    let file_auth = FileAuth(h.env.root)
    let sslctx =
      recover
        SSLContext
          .> set_authority(
            FilePath(file_auth, "assets/cert.pem"))?
          .> set_cert(
            FilePath(file_auth, "assets/cert.pem"),
            FilePath(file_auth, "assets/key.pem"))?
          .> set_client_verify(false)
          .> set_server_verify(false)
      end

    h.expect_action("not connected verified")
    h.expect_action("already tls verified")
    h.expect_action("not ready verified")

    let listener =
      _TestStartTLSPreconditionsListener(
        consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestStartTLSPreconditionsNotConnectedClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Tests StartTLSNotConnected by calling start_tls on a closed connection.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _h: TestHelper

  new create(port: String,
    sslctx: SSLContext val,
    h: TestHelper)
  =>
    _sslctx = sslctx
    _h = h

    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
        "localhost",
        port,
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _tcp_connection.close()
    match _tcp_connection.start_tls(_sslctx)
    | StartTLSNotConnected =>
      _h.complete_action("not connected verified")
    else
      _h.fail("Expected StartTLSNotConnected")
    end

actor \nodoc\ _TestStartTLSPreconditionsAlreadyTLSClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Tests StartTLSAlreadyTLS by calling start_tls on a connection that already
  has an SSL session. Connects plain, calls start_tls (which sets _ssl), then
  immediately calls start_tls again to get StartTLSAlreadyTLS.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _h: TestHelper

  new create(port: String,
    sslctx: SSLContext val,
    h: TestHelper)
  =>
    _sslctx = sslctx
    _h = h

    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
        "localhost",
        port,
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    // First start_tls sets _ssl
    match _tcp_connection.start_tls(_sslctx, "localhost")
    | None =>
      // _ssl is now set, second call should fail
      match _tcp_connection.start_tls(_sslctx, "localhost")
      | StartTLSAlreadyTLS =>
        _h.complete_action("already tls verified")
      else
        _h.fail("Expected StartTLSAlreadyTLS on second call")
      end
    else
      _h.fail("First start_tls should have succeeded")
    end

actor \nodoc\ _TestStartTLSPreconditionsNotReadyClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Tests StartTLSNotReady by calling start_tls on a muted connection.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _h: TestHelper

  new create(port: String,
    sslctx: SSLContext val,
    h: TestHelper)
  =>
    _sslctx = sslctx
    _h = h

    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
        "localhost",
        port,
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _tcp_connection.mute()
    match _tcp_connection.start_tls(_sslctx)
    | StartTLSNotReady =>
      _h.complete_action("not ready verified")
    else
      _h.fail("Expected StartTLSNotReady")
    end

actor \nodoc\ _TestStartTLSPreconditionsListener is TCPListenerActor
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _not_connected_client:
    (_TestStartTLSPreconditionsNotConnectedClient | None) = None
  var _already_tls_client:
    (_TestStartTLSPreconditionsAlreadyTLSClient | None) = None
  var _not_ready_client:
    (_TestStartTLSPreconditionsNotReadyClient | None) = None
  let _servers: Array[_TestDoNothingServerActor] =
    Array[_TestDoNothingServerActor]

  new create(sslctx: SSLContext val, h: TestHelper) =>
    _sslctx = sslctx
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        "9734",
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    let server = _TestDoNothingServerActor(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    try
      (_not_connected_client
        as _TestStartTLSPreconditionsNotConnectedClient).dispose()
    end
    try
      (_already_tls_client
        as _TestStartTLSPreconditionsAlreadyTLSClient).dispose()
    end
    try
      (_not_ready_client
        as _TestStartTLSPreconditionsNotReadyClient).dispose()
    end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listening() =>
    let port = "9734"
    _not_connected_client =
      _TestStartTLSPreconditionsNotConnectedClient(port, _sslctx, _h)
    _already_tls_client =
      _TestStartTLSPreconditionsAlreadyTLSClient(port, _sslctx, _h)
    _not_ready_client =
      _TestStartTLSPreconditionsNotReadyClient(port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestStartTLSPreconditionsListener")

class \nodoc\ iso _TestStartTLSSendDuringUpgrade is UnitTest
  """
  Test that send() returns SendErrorNotConnected during a TLS upgrade
  handshake (state: _TLSUpgrading), and that the refused send fires no
  `_on_send_accepted`. After start_tls() succeeds, the connection is in
  _TLSUpgrading where sends_allowed() = false.
  """
  fun name(): String => "net/StartTLSSendDuringUpgrade"

  fun apply(h: TestHelper) ? =>
    let port = "9760"
    let file_auth = FileAuth(h.env.root)
    let sslctx =
      recover
        SSLContext
          .> set_authority(
            FilePath(file_auth, "assets/cert.pem"))?
          .> set_cert(
            FilePath(file_auth, "assets/cert.pem"),
            FilePath(file_auth, "assets/key.pem"))?
          .> set_client_verify(false)
          .> set_server_verify(false)
      end

    h.expect_action("send blocked during upgrade")

    let listener =
      _TestStartTLSSendDuringUpgradeListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestStartTLSSendDuringUpgradeClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _h: TestHelper
  var _accepted_count: USize = 0

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _sslctx = sslctx
    _h = h

    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
        "localhost",
        port,
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    // Initiate TLS upgrade
    match \exhaustive\ _tcp_connection.start_tls(_sslctx, "localhost")
    | None =>
      // Now in _TLSUpgrading — send() should fail
      match \exhaustive\ _tcp_connection.send("should fail")
      | SendErrorNotConnected =>
        _h.assert_eq[USize](
          0,
          _accepted_count,
          "a refused send must not fire _on_send_accepted")
        _h.complete_action("send blocked during upgrade")
      | SendAccepted =>
        _h.fail("send() should not be accepted during TLS upgrade")
      | let _: SendError =>
        _h.fail("Expected SendErrorNotConnected, got other SendError")
      end
    | let _: StartTLSError =>
      _h.fail("start_tls should have succeeded")
    end

  fun ref _on_send_accepted(token: SendToken, data: (ByteSeq | ByteSeqIter)) =>
    _accepted_count = _accepted_count + 1

actor \nodoc\ _TestStartTLSSendDuringUpgradeListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestStartTLSSendDuringUpgradeClient | None) = None
  let _servers: Array[_TestDoNothingServerActor] =
    Array[_TestDoNothingServerActor]

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _port = port
    _sslctx = sslctx
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        _port,
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    let server = _TestDoNothingServerActor(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    for server in _servers.values() do server.dispose() end
    try (_client as _TestStartTLSSendDuringUpgradeClient).dispose() end

  fun ref _on_listening() =>
    _client =
      _TestStartTLSSendDuringUpgradeClient(
        _port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestStartTLSSendDuringUpgradeListener")

class \nodoc\ iso _TestStartTLSHandshakeFailure is UnitTest
  """
  Test that a TLS upgrade handshake failure fires `_on_tls_failure` followed
  by `_on_closed` via `_hard_close_tls_upgrading`. Client initiates STARTTLS,
  the server replies "OK" and sends garbage instead of doing the TLS
  handshake, causing the client's TLS upgrade to fail.
  """
  fun name(): String => "net/StartTLSHandshakeFailure"

  fun apply(h: TestHelper) ? =>
    let port = "9761"
    let file_auth = FileAuth(h.env.root)
    let sslctx =
      recover
        SSLContext
          .> set_authority(
            FilePath(file_auth, "assets/cert.pem"))?
          .> set_cert(
            FilePath(file_auth, "assets/cert.pem"),
            FilePath(file_auth, "assets/key.pem"))?
          .> set_client_verify(false)
          .> set_server_verify(false)
      end

    h.expect_action("tls failure received")
    h.expect_action("on closed received")

    let listener =
      _TestStartTLSHandshakeFailureListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestStartTLSHandshakeFailureClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Plaintext client that negotiates STARTTLS, then upgrades. The server sends
  garbage after "OK", so the TLS handshake fails.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _h: TestHelper

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _sslctx = sslctx
    _h = h

    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
        "localhost",
        port,
        "",
        this,
        this)
    match MakeBufferSize(2)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _tcp_connection.send("STARTTLS")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let msg = String.from_array(consume data)
    if msg == "OK" then
      match _tcp_connection.start_tls(_sslctx, "localhost")
      | let _: StartTLSError =>
        _h.fail("Client start_tls should have succeeded")
      end
    else
      _h.fail("Client got unexpected: " + msg)
    end
    KeepReading

  fun ref _on_tls_ready() =>
    _h.fail("TLS handshake should not have succeeded")

  fun ref _on_tls_failure(reason: TLSFailureReason) =>
    _h.complete_action("tls failure received")

  fun ref _on_closed() =>
    _h.complete_action("on closed received")

actor \nodoc\ _TestStartTLSHandshakeFailureServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Plaintext server that responds to "STARTTLS" with "OK", waits for the
  client's ClientHello (which arrives as binary data), then sends garbage
  to break the TLS handshake. The wait ensures "OK" is consumed by the
  client before garbage arrives, avoiding a `StartTLSHasBufferedData`
  precondition failure (CVE-2021-23222 check).
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper
  var _awaiting_client_hello: Bool = false

  new create(fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(_h.env.root),
        fd,
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _awaiting_client_hello then
      // Client sent a ClientHello — respond with garbage to break the
      // handshake.
      _tcp_connection.send("XXXXXXXXXX")
      _tcp_connection.close()
    else
      let msg = String.from_array(consume data)
      if msg == "STARTTLS" then
        _tcp_connection.send("OK")
        _awaiting_client_hello = true
      end
    end
    KeepReading

actor \nodoc\ _TestStartTLSHandshakeFailureListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestStartTLSHandshakeFailureClient | None) = None
  let _servers: Array[_TestStartTLSHandshakeFailureServer] =
    Array[_TestStartTLSHandshakeFailureServer]

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _port = port
    _sslctx = sslctx
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        _port,
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestStartTLSHandshakeFailureServer =>
    let server = _TestStartTLSHandshakeFailureServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    for server in _servers.values() do server.dispose() end
    try (_client as _TestStartTLSHandshakeFailureClient).dispose() end

  fun ref _on_listening() =>
    _client =
      _TestStartTLSHandshakeFailureClient(
        _port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestStartTLSHandshakeFailureListener")

class \nodoc\ iso _TestStartTLSIsWriteableDuringUpgrade is UnitTest
  """
  Test that is_writeable() returns false during a TLS upgrade handshake
  (state: _TLSUpgrading). After start_tls() succeeds, the connection is
  in _TLSUpgrading where sends_allowed() = false.
  """
  fun name(): String => "net/StartTLSIsWriteableDuringUpgrade"

  fun apply(h: TestHelper) ? =>
    let port = "9762"
    let file_auth = FileAuth(h.env.root)
    let sslctx =
      recover
        SSLContext
          .> set_authority(
            FilePath(file_auth, "assets/cert.pem"))?
          .> set_cert(
            FilePath(file_auth, "assets/cert.pem"),
            FilePath(file_auth, "assets/key.pem"))?
          .> set_client_verify(false)
          .> set_server_verify(false)
      end

    h.expect_action("is_writeable false during upgrade")

    let listener =
      _TestStartTLSIsWriteableListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestStartTLSIsWriteableClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _h: TestHelper

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _sslctx = sslctx
    _h = h

    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
        "localhost",
        port,
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    // Initiate TLS upgrade
    match \exhaustive\ _tcp_connection.start_tls(_sslctx, "localhost")
    | None =>
      // Now in _TLSUpgrading — is_writeable() should be false
      _h.assert_false(
        _tcp_connection.is_writeable(),
        "is_writeable should be false during TLS upgrade")
      _h.complete_action("is_writeable false during upgrade")
    | let _: StartTLSError =>
      _h.fail("start_tls should have succeeded")
    end

actor \nodoc\ _TestStartTLSIsWriteableListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestStartTLSIsWriteableClient | None) = None
  let _servers: Array[_TestDoNothingServerActor] =
    Array[_TestDoNothingServerActor]

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _port = port
    _sslctx = sslctx
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        _port,
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestDoNothingServerActor =>
    let server = _TestDoNothingServerActor(fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    for server in _servers.values() do server.dispose() end
    try (_client as _TestStartTLSIsWriteableClient).dispose() end

  fun ref _on_listening() =>
    _client =
      _TestStartTLSIsWriteableClient(
        _port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestStartTLSIsWriteableListener")

class \nodoc\ iso _TestStartTLSAuthFailure is UnitTest
  """
  Test that a TLS upgrade with a hostname verification failure fires
  `_on_tls_failure(TLSAuthFailed)` followed by `_on_closed` via
  `_hard_close_tls_upgrading`. Client initiates STARTTLS, both sides upgrade,
  but the client uses `set_client_verify(true)` with a hostname that doesn't
  match the server certificate's SAN, triggering `SSLAuthFail`.
  """
  fun name(): String => "net/StartTLSAuthFailure"

  fun apply(h: TestHelper) ? =>
    let port = "9764"
    let file_auth = FileAuth(h.env.root)
    let server_sslctx =
      recover
        SSLContext
          .> set_cert(
            FilePath(file_auth, "assets/cert.pem"),
            FilePath(file_auth, "assets/key.pem"))?
          .> set_client_verify(false)
          .> set_server_verify(false)
      end
    let client_sslctx =
      recover
        SSLContext
          .> set_authority(
            FilePath(file_auth, "assets/cert.pem"))?
          .> set_client_verify(true)
          .> set_server_verify(false)
      end

    h.expect_action("tls auth failure received")
    h.expect_action("on closed received")

    let listener =
      _TestStartTLSAuthFailureListener(
        port, consume server_sslctx, consume client_sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestStartTLSAuthFailureClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Plaintext client that negotiates STARTTLS, then upgrades with
  `set_client_verify(true)` and a hostname that doesn't match the server
  certificate's SAN, triggering `SSLAuthFail`.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _h: TestHelper

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _sslctx = sslctx
    _h = h

    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
        "localhost",
        port,
        "",
        this,
        this)
    match MakeBufferSize(2)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _tcp_connection.send("STARTTLS")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let msg = String.from_array(consume data)
    if msg == "OK" then
      match _tcp_connection.start_tls(_sslctx, "not.localhost")
      | let _: StartTLSError =>
        _h.fail("Client start_tls should have succeeded")
      end
    else
      _h.fail("Client got unexpected: " + msg)
    end
    KeepReading

  fun ref _on_tls_ready() =>
    _h.fail("TLS handshake should not have succeeded")

  fun ref _on_tls_failure(reason: TLSFailureReason) =>
    match \exhaustive\ reason
    | TLSAuthFailed =>
      _h.complete_action("tls auth failure received")
    | TLSGeneralError =>
      _h.fail("Expected TLSAuthFailed, got TLSGeneralError")
    end

  fun ref _on_closed() =>
    _h.complete_action("on closed received")

actor \nodoc\ _TestStartTLSAuthFailureServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Plaintext server that responds to "STARTTLS" with "OK" and upgrades to TLS.
  Tolerant of success or failure — the client may tear down the connection
  after hostname verification fails.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _h: TestHelper

  new create(sslctx: SSLContext val, fd: U32, h: TestHelper) =>
    _sslctx = sslctx
    _h = h

    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(_h.env.root),
        fd,
        this,
        this)
    match MakeBufferSize(8)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let msg = String.from_array(consume data)
    if msg == "STARTTLS" then
      _tcp_connection.send("OK")
      match _tcp_connection.start_tls(_sslctx)
      | let _: StartTLSError =>
        _h.fail("Server start_tls failed")
        _h.complete(false)
      end
    end
    KeepReading

actor \nodoc\ _TestStartTLSAuthFailureListener is TCPListenerActor
  let _port: String
  let _server_sslctx: SSLContext val
  let _client_sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestStartTLSAuthFailureClient | None) = None
  let _servers: Array[_TestStartTLSAuthFailureServer] =
    Array[_TestStartTLSAuthFailureServer]

  new create(port: String,
    server_sslctx: SSLContext val,
    client_sslctx: SSLContext val,
    h: TestHelper)
  =>
    _port = port
    _server_sslctx = server_sslctx
    _client_sslctx = client_sslctx
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        _port,
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestStartTLSAuthFailureServer =>
    let server = _TestStartTLSAuthFailureServer(_server_sslctx, fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    for server in _servers.values() do server.dispose() end
    try (_client as _TestStartTLSAuthFailureClient).dispose() end

  fun ref _on_listening() =>
    _client =
      _TestStartTLSAuthFailureClient(
        _port, _client_sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestStartTLSAuthFailureListener")

class \nodoc\ iso _TestSetTimerAfterTLSUpgrade is UnitTest
  """
  Test that set_timer() succeeds after a TLS upgrade and the timer fires.
  Client connects plaintext, negotiates STARTTLS, upgrades to TLS, then
  calls set_timer() in _on_tls_ready and verifies it returns a TimerToken
  and fires with the correct token.
  """
  fun name(): String => "net/SetTimerAfterTLSUpgrade"

  fun apply(h: TestHelper) ? =>
    let port = "9765"
    let file_auth = FileAuth(h.env.root)
    let sslctx =
      recover
        SSLContext
          .> set_authority(
            FilePath(file_auth, "assets/cert.pem"))?
          .> set_cert(
            FilePath(file_auth, "assets/cert.pem"),
            FilePath(file_auth, "assets/key.pem"))?
          .> set_client_verify(false)
          .> set_server_verify(false)
      end

    h.expect_action("set_timer succeeded")
    h.expect_action("timer fired")

    let listener =
      _TestSetTimerAfterTLSUpgradeListener(
        port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestSetTimerAfterTLSUpgradeClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _h: TestHelper
  var _expected_token: (TimerToken | None) = None

  new create(port: String,
    sslctx: SSLContext val,
    h: TestHelper)
  =>
    _sslctx = sslctx
    _h = h

    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
        "localhost",
        port,
        "",
        this,
        this)
    match MakeBufferSize(2)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _tcp_connection.send("STARTTLS")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let msg = String.from_array(consume data)
    if msg == "OK" then
      match _tcp_connection.start_tls(_sslctx, "localhost")
      | let _: StartTLSError =>
        _h.fail("Client start_tls failed")
        _h.complete(false)
      end
    else
      _h.fail("Client got unexpected: " + msg)
    end
    KeepReading

  fun ref _on_tls_ready() =>
    match \exhaustive\ MakeTimerDuration(2_000)
    | let d: TimerDuration =>
      match \exhaustive\ _tcp_connection.set_timer(d)
      | let t: TimerToken =>
        _expected_token = t
        _h.complete_action("set_timer succeeded")
      | let _: SetTimerError =>
        _h.fail("set_timer returned error after TLS upgrade")
        _h.complete(false)
      end
    | let _: ValidationFailure =>
      _h.fail("MakeTimerDuration(2_000) should succeed")
      _h.complete(false)
    end

  fun ref _on_timer(token: TimerToken) =>
    match _expected_token
    | let t: TimerToken =>
      _h.assert_true(t == token, "token should match")
      _h.complete_action("timer fired")
    else
      _h.fail("_on_timer fired without expected token")
      _h.complete(false)
    end

  fun ref _on_tls_failure(reason: TLSFailureReason) =>
    _h.fail("Client TLS handshake failed")

actor \nodoc\ _TestSetTimerAfterTLSUpgradeServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _h: TestHelper

  new create(sslctx: SSLContext val,
    fd: U32,
    h: TestHelper)
  =>
    _sslctx = sslctx
    _h = h

    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(_h.env.root),
        fd,
        this,
        this)
    match MakeBufferSize(8)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let msg = String.from_array(consume data)
    if msg == "STARTTLS" then
      _tcp_connection.send("OK")
      match _tcp_connection.start_tls(_sslctx)
      | let _: StartTLSError =>
        _h.fail("Server start_tls failed")
        _h.complete(false)
      end
    else
      _h.fail("Server got unexpected: " + msg)
    end
    KeepReading

  fun ref _on_tls_failure(reason: TLSFailureReason) =>
    _h.fail("Server TLS handshake failed")

actor \nodoc\ _TestSetTimerAfterTLSUpgradeListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestSetTimerAfterTLSUpgradeClient | None) = None
  let _servers: Array[_TestSetTimerAfterTLSUpgradeServer] =
    Array[_TestSetTimerAfterTLSUpgradeServer]

  new create(port: String,
    sslctx: SSLContext val,
    h: TestHelper)
  =>
    _port = port
    _sslctx = sslctx
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        _port,
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSetTimerAfterTLSUpgradeServer =>
    let server = _TestSetTimerAfterTLSUpgradeServer(_sslctx, fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    for server in _servers.values() do server.dispose() end
    try (_client as _TestSetTimerAfterTLSUpgradeClient).dispose() end

  fun ref _on_listening() =>
    _client =
      _TestSetTimerAfterTLSUpgradeClient(
        _port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestSetTimerAfterTLSUpgradeListener")

class \nodoc\ iso _TestStartTLSHardCloseOnTLSReady is UnitTest
  """
  Test that hard_close() from inside _on_tls_ready is safe. _on_tls_ready
  fires from `_ssl_poll()` via `_TLSUpgrading.ssl_handshake_complete()`,
  which moves the connection to `_Open` before the callback runs. The
  flush below the callback must not touch the disposed session.
  """
  fun name(): String => "net/StartTLSHardCloseOnTLSReady"

  fun apply(h: TestHelper) ? =>
    let port = "9776"
    let file_auth = FileAuth(h.env.root)
    let sslctx =
      recover
        SSLContext
          .> set_authority(
            FilePath(file_auth, "assets/cert.pem"))?
          .> set_cert(
            FilePath(file_auth, "assets/cert.pem"),
            FilePath(file_auth, "assets/key.pem"))?
          .> set_client_verify(false)
          .> set_server_verify(false)
      end

    h.expect_action("client tls ready")
    h.expect_action("client closed")

    let listener = _TestStartTLSHardCloseListener(port, consume sslctx, h)
    h.dispose_when_done(listener)

    h.long_test(5_000_000_000)

actor \nodoc\ _TestStartTLSHardCloseListener is TCPListenerActor
  let _port: String
  let _sslctx: SSLContext val
  var _tcp_listener: TCPListener = TCPListener.none()
  let _h: TestHelper
  var _client: (_TestStartTLSHardCloseClient | None) = None
  let _servers: Array[_TestStartTLSHardCloseServer] =
    Array[_TestStartTLSHardCloseServer]

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _port = port
    _sslctx = sslctx
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(_h.env.root),
        "localhost",
        _port,
        this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestStartTLSHardCloseServer =>
    let server = _TestStartTLSHardCloseServer(_sslctx, fd, _h)
    _servers.push(server)
    server

  fun ref _on_closed() =>
    for server in _servers.values() do server.dispose() end
    try (_client as _TestStartTLSHardCloseClient).dispose() end

  fun ref _on_listening() =>
    _client = _TestStartTLSHardCloseClient(_port, _sslctx, _h)

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open _TestStartTLSHardCloseListener")

actor \nodoc\ _TestStartTLSHardCloseClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _h: TestHelper

  new create(port: String, sslctx: SSLContext val, h: TestHelper) =>
    _sslctx = sslctx
    _h = h
    _tcp_connection =
      TCPConnection.client(
        TCPConnectAuth(h.env.root),
        "localhost",
        port,
        "",
        this,
        this)
    match MakeBufferSize(2)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    _tcp_connection.send("STARTTLS")

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let msg = String.from_array(consume data)
    if msg == "OK" then
      match _tcp_connection.start_tls(_sslctx, "localhost")
      | let _: StartTLSError =>
        _h.fail("Client start_tls failed")
        _h.complete(false)
      end
    else
      _h.fail("Client got unexpected: " + msg)
    end
    KeepReading

  fun ref _on_tls_ready() =>
    _h.complete_action("client tls ready")
    _tcp_connection.hard_close()

  fun ref _on_tls_failure(reason: TLSFailureReason) =>
    _h.fail("Client TLS handshake failed")

  fun ref _on_closed() =>
    _h.complete_action("client closed")

actor \nodoc\ _TestStartTLSHardCloseServer
  is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  The client hard-closes as soon as its TLS upgrade completes, so this server
  may finish its own upgrade, fail it, or just close. None of that is under
  test.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _sslctx: SSLContext val
  let _h: TestHelper

  new create(sslctx: SSLContext val, fd: U32, h: TestHelper) =>
    _sslctx = sslctx
    _h = h
    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(_h.env.root),
        fd,
        this,
        this)
    match MakeBufferSize(8)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    let msg = String.from_array(consume data)
    if msg == "STARTTLS" then
      _tcp_connection.send("OK")
      match _tcp_connection.start_tls(_sslctx)
      | let _: StartTLSError =>
        _h.fail("Server start_tls failed")
        _h.complete(false)
      end
    else
      _h.fail("Server got unexpected: " + msg)
    end
    KeepReading
