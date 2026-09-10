"""
Client that demonstrates connection timeout.

Connects to 192.0.2.1 (RFC 5737 TEST-NET-1), a non-routable address that
black-holes SYN packets. A 3-second connection timeout bounds the attempt.
When the timeout fires, `_on_connection_failure` receives
`ConnectionFailedTimeout` and the program exits.
"""
use "net"

actor Main
  new create(env: Env) =>
    TimeoutClient(TCPConnectAuth(env.root), env.out)

actor TimeoutClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Connects to a non-routable address with a connection timeout and prints the
  resulting connection failure.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _out: OutStream

  new create(auth: TCPConnectAuth, out: OutStream) =>
    _out = out
    _out.print("Connecting to 192.0.2.1:7677 with 3-second timeout...")
    match MakeConnectionTimeout(3_000)
    | let ct: ConnectionTimeout =>
      _tcp_connection =
        TCPConnection.client(
          auth,
          "192.0.2.1",
          "7677",
          "",
          this,
          this
          where connection_timeout = ct)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _out.print("Connected (unexpected for a non-routable address).")
    _tcp_connection.close()

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    match \exhaustive\ reason
    | ConnectionFailedTimeout =>
      _out.print("Connection timed out (expected).")
    | ConnectionFailedDNS =>
      _out.print("DNS resolution failed.")
    | ConnectionFailedTCP =>
      _out.print("All TCP connection attempts failed.")
    | ConnectionFailedSSL =>
      _out.print("SSL handshake failed.")
    | ConnectionFailedTimerError =>
      _out.print("Connect timer subscription failed.")
    end
