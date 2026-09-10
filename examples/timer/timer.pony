"""
Query-timeout simulation using `set_timer()`.

A client connects to a server, sends a "query", and sets a 3-second timer.
The server receives the query but never responds. When the timer fires,
`_on_timer()` logs the timeout and closes the connection.

This demonstrates the motivating use case for the general-purpose timer API:
a timeout that fires unconditionally, regardless of I/O activity. Unlike
`idle_timeout()`, which resets on every send/receive, `set_timer()` fires after
the configured duration no matter what the wire is doing.
"""
use "net"

actor Main
  new create(env: Env) =>
    let listen_auth = TCPListenAuth(env.root)
    let connect_auth = TCPConnectAuth(env.root)
    QueryTimeoutListener(listen_auth, connect_auth, env.out)

actor QueryTimeoutListener is TCPListenerActor
  """
  Listens on the example's port and starts the client once listening.
  """
  var _tcp_listener: TCPListener = TCPListener.none()
  let _out: OutStream
  let _server_auth: TCPServerAuth
  let _connect_auth: TCPConnectAuth

  new create(listen_auth: TCPListenAuth,
    connect_auth: TCPConnectAuth,
    out: OutStream)
  =>
    _out = out
    _server_auth = TCPServerAuth(listen_auth)
    _connect_auth = connect_auth
    _tcp_listener = TCPListener(listen_auth, "127.0.0.1", "7678", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): SlowServer =>
    SlowServer(_server_auth, fd, _out)

  fun ref _on_closed() =>
    _out.print("Listener shut down.")

  fun ref _on_listen_failure() =>
    _out.print("Couldn't start listener. Perhaps try another port?")

  fun ref _on_listening() =>
    _out.print("Listening on port 7678. Launching client...")
    QueryTimeoutClient(_connect_auth, _out)

actor SlowServer is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Receives the query and never responds.
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

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    _out.print("[server] Received query. Pretending to think forever...")
    // Never responds — simulating a slow query
    KeepReading

  fun ref _on_closed() =>
    _out.print("[server] Connection closed.")

actor QueryTimeoutClient
  is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Sends a query and closes the connection when its query timer fires.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _out: OutStream
  var _query_timer: (TimerToken | None) = None

  new create(auth: TCPConnectAuth, out: OutStream) =>
    _out = out
    _tcp_connection =
      TCPConnection.client(
        auth,
        "127.0.0.1",
        "7678",
        "",
        this,
        this)

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_connected() =>
    _out.print("[client] Connected. Sending query with 3-second timeout...")
    _tcp_connection.send("SELECT * FROM big_table")
    match MakeTimerDuration(3_000)
    | let d: TimerDuration =>
      match \exhaustive\ _tcp_connection.set_timer(d)
      | let t: TimerToken => _query_timer = t
      | let _: SetTimerError =>
        _out.print("[client] Failed to set timer.")
      end
    end

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    // Response arrived — cancel the query timeout
    match _query_timer
    | let t: TimerToken =>
      _tcp_connection.cancel_timer(t)
      _query_timer = None
    end
    _out.print("[client] Got response (this won't happen in this example).")
    KeepReading

  fun ref _on_timer(token: TimerToken) =>
    match _query_timer
    | let t: TimerToken if t == token =>
      _query_timer = None
      _out.print("[client] Query timed out after 3 seconds! Closing.")
      _tcp_connection.close()
    end

  fun ref _on_timer_failure() =>
    _query_timer = None
    _out.print("[client] Query timer subscription failed. No timeout armed.")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _out.print("[client] Connection failed.")

  fun ref _on_closed() =>
    _out.print("[client] Connection closed.")
