"""
Demonstrates configurable read buffer sizing with two phases: a small control
phase and a bulk transfer phase.

The client connects and sends a 4-byte command header ("BULK") followed by a
large payload. The server starts with a small read buffer minimum (128 bytes)
for the control phase, then resizes to a larger buffer (8192 bytes) for the
bulk transfer phase using `set_read_buffer_minimum()` and
`resize_read_buffer()`.
"""
use "constrained_types"
use "net"

actor Main
  new create(env: Env) =>
    Listener(TCPListenAuth(env.root), TCPConnectAuth(env.root), env.out)

actor Listener is TCPListenerActor
  """
  Listens on the example's port, creating a server for each accepted connection
  and launching the client once listening.
  """
  var _tcp_listener: TCPListener = TCPListener.none()
  let _out: OutStream
  let _connect_auth: TCPConnectAuth
  let _server_auth: TCPServerAuth

  new create(listen_auth: TCPListenAuth,
    connect_auth: TCPConnectAuth,
    out: OutStream)
  =>
    _connect_auth = connect_auth
    _out = out
    _server_auth = TCPServerAuth(listen_auth)
    _tcp_listener = TCPListener(listen_auth, "127.0.0.1", "7675", this)

  fun ref _listener(): TCPListener =>
    _tcp_listener

  fun ref _on_accept(fd: U32): Server =>
    Server(_server_auth, fd, _out)

  fun ref _on_listening() =>
    _out.print("Listener ready, launching client...")
    Client(_connect_auth, "127.0.0.1", "7675", _out)

  fun ref _on_listen_failure() =>
    _out.print("Unable to open listener")

actor Server is (TCPConnectionActor & ServerLifecycleEventReceiver)
  """
  Server that starts with a small read buffer for the control phase, then
  resizes for bulk transfer after receiving the "BULK" command.
  """
  var _tcp_connection: TCPConnection = TCPConnection.none()
  let _out: OutStream
  var _control_phase: Bool = true

  new create(auth: TCPServerAuth, fd: U32, out: OutStream) =>
    _out = out
    // Start with a small 128-byte buffer for the control phase
    match \exhaustive\ MakeReadBufferSize(128)
    | let rbs: ReadBufferSize =>
      _tcp_connection =
        TCPConnection.server(
          auth, fd, this, this
          where read_buffer_size = rbs)
    | let _: ValidationFailure =>
      _tcp_connection = TCPConnection.server(auth, fd, this, this)
    end
    match MakeBufferSize(4)
    | let e: BufferSize => _tcp_connection.buffer_until(e)
    end

  fun ref _connection(): TCPConnection =>
    _tcp_connection

  fun ref _on_start_failure(reason: StartFailureReason) =>
    None

  fun ref _on_received(data: Array[U8] iso): ReadAction =>
    if _control_phase then
      let cmd = String.from_array(consume data)
      _out.print("Server: received command \"" + cmd + "\"")

      if cmd == "BULK" then
        _out.print("Server: switching to bulk transfer mode")
        // Raise the buffer minimum and resize for bulk transfer
        match MakeReadBufferSize(8192)
        | let rbs: ReadBufferSize =>
          _tcp_connection.set_read_buffer_minimum(rbs)
          _tcp_connection.resize_read_buffer(rbs)
        end
        // Read all available data in bulk mode
        _tcp_connection.buffer_until(Streaming)
        _control_phase = false
      end
    else
      _out.print("Server: received " + data.size().string() +
        " bytes of bulk data")
      _tcp_connection.close()
    end
    KeepReading

  fun ref _on_closed() =>
    _out.print("Server: closed")

actor Client is (TCPConnectionActor & ClientLifecycleEventReceiver)
  """
  Client that sends a control command followed by bulk data.
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

  fun ref _on_connected() =>
    _out.print("Client: connected")
    // Send control command
    _tcp_connection.send("BULK")
    // Send bulk data (1KB payload)
    let payload =
      recover val
        let a = Array[U8](1024)
        var i: USize = 0
        while i < 1024 do
          a.push('X')
          i = i + 1
        end
        a
      end
    _tcp_connection.send(payload)
    _out.print("Client: sent BULK command + 1024 bytes of data")

  fun ref _on_connection_failure(reason: ConnectionFailureReason) =>
    _out.print("Client: connection failed")

  fun ref _on_closed() =>
    _out.print("Client: closed")
