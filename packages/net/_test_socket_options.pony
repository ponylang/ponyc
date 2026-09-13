use "pony_test"

class \nodoc\ iso _TestSocketOptionsConnected is UnitTest
  """
  Test that socket option methods succeed on a connected socket, including
  convenience methods (set_nodelay, set_so_rcvbuf, etc.) and general-purpose
  getsockopt/setsockopt/getsockopt_u32/setsockopt_u32.
  """
  fun name(): String => "net/SocketOptionsConnected"

  fun apply(h: TestHelper) =>
    let listener = _TestSocketOptionsListener(h)
    h.dispose_when_done(listener)
    h.long_test(5_000_000_000)

actor \nodoc\ _TestSocketOptionsListener is TCPListenerActor
  var _tcp_listener: TCPListener = TCPListener.none()
  var _client: (_TestSocketOptionsClient | None) = None
  let _servers: Array[_TestSocketOptionsServer] =
    Array[_TestSocketOptionsServer]
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h
    _tcp_listener =
      TCPListener(
        TCPListenAuth(h.env.root), "127.0.0.1", "7708", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): _TestSocketOptionsServer =>
    let server = _TestSocketOptionsServer(fd, _h)
    _servers.push(server)
    server

  fun ref _on_listening() =>
    _client =
      _TestSocketOptionsClient(
        TCPConnectAuth(_h.env.root), "127.0.0.1", "7708")

  fun ref _on_closed() =>
    try (_client as _TestSocketOptionsClient).dispose() end
    for server in _servers.values() do server.dispose() end

  fun ref _on_listen_failure() =>
    _h.fail("Unable to open listener")

actor \nodoc\ _TestSocketOptionsServer is
  (TCPConnectionActor & ServerLifecycleEventReceiver)
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _h: TestHelper

  new create(fd: U32, h: TestHelper) =>
    _h = h
    _tcp_connection =
      TCPConnection.server(
        TCPServerAuth(h.env.root), fd, this, this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_started() =>
    // set_nodelay: enable and disable should both succeed
    _h.assert_eq[U32](
      0,
      _tcp_connection.set_nodelay(true),
      "set_nodelay(true) should succeed")
    _h.assert_eq[U32](
      0,
      _tcp_connection.set_nodelay(false),
      "set_nodelay(false) should succeed")

    // set_so_rcvbuf: set then get. OS may round up, so check >= requested.
    let rcvbuf_result = _tcp_connection.set_so_rcvbuf(8192)
    _h.assert_eq[U32](0, rcvbuf_result, "set_so_rcvbuf should succeed")
    (let rcv_errno: U32, let rcv_size: U32) =
      _tcp_connection.get_so_rcvbuf()
    _h.assert_eq[U32](0, rcv_errno, "get_so_rcvbuf errno should be 0")
    _h.assert_true(
      rcv_size >= 8192,
      "get_so_rcvbuf should return >= 8192, got " + rcv_size.string())

    // set_so_sndbuf: set then get. OS may round up, so check >= requested.
    let sndbuf_result = _tcp_connection.set_so_sndbuf(8192)
    _h.assert_eq[U32](0, sndbuf_result, "set_so_sndbuf should succeed")
    (let snd_errno: U32, let snd_size: U32) =
      _tcp_connection.get_so_sndbuf()
    _h.assert_eq[U32](0, snd_errno, "get_so_sndbuf errno should be 0")
    _h.assert_true(
      snd_size >= 8192,
      "get_so_sndbuf should return >= 8192, got " + snd_size.string())

    // setsockopt_u32/getsockopt_u32: set SO_RCVBUF via general method,
    // read back via general method.
    let gen_set_result =
      _tcp_connection.setsockopt_u32(
        OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf(), 16384)
    _h.assert_eq[U32](
      0,
      gen_set_result,
      "setsockopt_u32 SO_RCVBUF should succeed")
    (let gen_get_errno: U32, let gen_get_size: U32) =
      _tcp_connection.getsockopt_u32(
        OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf())
    _h.assert_eq[U32](
      0,
      gen_get_errno,
      "getsockopt_u32 SO_RCVBUF errno should be 0")
    _h.assert_true(
      gen_get_size >= 16384,
      "getsockopt_u32 SO_RCVBUF should return >= 16384, got " +
        gen_get_size.string())

    // setsockopt/getsockopt: set SO_SNDBUF via raw bytes, read back via
    // raw bytes.
    let raw_set_result =
      _tcp_connection.setsockopt(
        OSSockOpt.sol_socket(),
        OSSockOpt.so_sndbuf(),
        Array[U8](4) .> push_u32(16384))
    _h.assert_eq[U32](
      0,
      raw_set_result,
      "setsockopt SO_SNDBUF should succeed")
    (let raw_get_errno: U32, let raw_get_bytes: Array[U8] iso) =
      _tcp_connection.getsockopt(
        OSSockOpt.sol_socket(), OSSockOpt.so_sndbuf())
    _h.assert_eq[U32](
      0,
      raw_get_errno,
      "getsockopt SO_SNDBUF errno should be 0")
    try
      let raw_get_size = (consume raw_get_bytes).read_u32(0)?
      _h.assert_true(
        raw_get_size >= 16384,
        "getsockopt SO_SNDBUF should return >= 16384, got " +
          raw_get_size.string())
    else
      _h.fail("getsockopt SO_SNDBUF returned too few bytes")
    end

    _h.complete(true)
    _tcp_connection.close()

class \nodoc\ iso _TestSocketOptionsNotConnected is UnitTest
  """
  Test that socket option methods return non-zero errno on a connection
  that is not open, including both convenience methods and general-purpose
  getsockopt/setsockopt.
  """
  fun name(): String => "net/SocketOptionsNotConnected"

  fun apply(h: TestHelper) =>
    let conn = TCPConnection.none()

    h.assert_true(
      conn.set_nodelay(true) != 0,
      "set_nodelay on none should return non-zero")
    h.assert_true(
      conn.set_so_rcvbuf(8192) != 0,
      "set_so_rcvbuf on none should return non-zero")
    h.assert_true(
      conn.set_so_sndbuf(8192) != 0,
      "set_so_sndbuf on none should return non-zero")

    (let rcv_errno: U32, _) = conn.get_so_rcvbuf()
    h.assert_true(
      rcv_errno != 0,
      "get_so_rcvbuf on none should return non-zero errno")
    (let snd_errno: U32, _) = conn.get_so_sndbuf()
    h.assert_true(
      snd_errno != 0,
      "get_so_sndbuf on none should return non-zero errno")

    // General-purpose methods
    h.assert_true(
      conn.setsockopt_u32(
        OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf(), 8192) != 0,
      "setsockopt_u32 on none should return non-zero")
    h.assert_true(
      conn.setsockopt(
        OSSockOpt.sol_socket(),
        OSSockOpt.so_rcvbuf(),
        Array[U8](4) .> push_u32(8192)) != 0,
      "setsockopt on none should return non-zero")

    (let gen_u32_errno: U32, _) =
      conn.getsockopt_u32(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf())
    h.assert_true(
      gen_u32_errno != 0,
      "getsockopt_u32 on none should return non-zero errno")
    (let gen_errno: U32, _) =
      conn.getsockopt(OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf())
    h.assert_true(
      gen_errno != 0,
      "getsockopt on none should return non-zero errno")

actor \nodoc\ _TestSocketOptionsClient is
  (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Client that connects and stays connected until disposed. Unlike
  _TestReadBufferTriggerClient, this does not close on connect — OpenBSD
  rejects setsockopt(SO_RCVBUF) once a FIN has been delivered to the
  receive buffer, so closing the client before the server tests socket
  options causes a race.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()

  new create(auth: TCPConnectAuth, host: String, port: String) =>
    _tcp_connection = TCPConnection.client(auth, host, port, "", this, this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    None

  fun ref _on_connected() =>
    None

  be dispose() =>
    _tcp_connection.close()
