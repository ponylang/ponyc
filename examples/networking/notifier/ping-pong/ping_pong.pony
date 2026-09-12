"""
Client and server exchanging messages using the notifier API.

Shows the same infinite ping-pong behavior as the `infinite-ping-pong` example,
but with notifier traits instead of class-and-trait delegation. The
listener creates a client once listening, the client sends "Ping" on connect,
and both sides exchange 4-byte messages forever.

Demonstrates `TCPListenNotify`, `ClientTCPConnectionNotify`, and
`ServerTCPConnectionNotify` working together. Both sides use `buffer_until()`
so each `on_received` callback delivers exactly one 4-byte message.
"""
use "constrained_types"
use "net"
use notifier = "net/notifier"

actor Main
  new create(env: Env) =>
    notifier.TCPListener(
      TCPListenAuth(env.root),
      recover PingPongListenNotify(TCPConnectAuth(env.root), env.out) end,
      "127.0.0.1",
      "7680")

class PingPongListenNotify is notifier.TCPListenNotify
  """
  Listener callbacks: launches a PingNotify client on listen and creates a
  PongNotify for each accepted connection.
  """
  let _connect_auth: TCPConnectAuth
  let _out: OutStream

  new create(connect_auth: TCPConnectAuth, out: OutStream) =>
    _connect_auth = connect_auth
    _out = out

  fun ref on_listening(listen: notifier.TCPListener ref) =>
    notifier.ClientTCPConnection(
      _connect_auth,
      recover PingNotify(_out) end,
      "127.0.0.1",
      "7680")

  fun ref on_not_listening(listen: notifier.TCPListener ref) =>
    _out.print("Unable to open listener")

  fun ref on_connected(listen: notifier.TCPListener ref):
    notifier.ServerTCPConnectionNotify iso^
  =>
    recover PongNotify(_out) end

class PingNotify is notifier.ClientTCPConnectionNotify
  """
  Client callbacks: sends "Ping" on connect and after each received reply.
  """
  let _out: OutStream

  new create(out: OutStream) =>
    _out = out

  fun ref on_connected(conn: notifier.ClientTCPConnection ref) =>
    match MakeBufferSize(4)
    | let e: BufferSize => conn.buffer_until(e)
    end
    conn.write("Ping")

  fun ref on_received(conn: notifier.ClientTCPConnection ref,
    data: Array[U8] iso): ReadAction
  =>
    _out.print(consume data)
    conn.write("Ping")
    KeepReading

  fun ref on_connect_failed(conn: notifier.ClientTCPConnection ref,
    reason: ConnectionFailureReason)
  =>
    _out.print("Unable to connect")

class PongNotify is notifier.ServerTCPConnectionNotify
  """
  Server callbacks: replies "Pong" to each received message.
  """
  let _out: OutStream

  new create(out: OutStream) =>
    _out = out

  fun ref on_start_failure(conn: notifier.ServerTCPConnection ref,
    reason: StartFailureReason)
  =>
    None

  fun ref on_accepted(conn: notifier.ServerTCPConnection ref) =>
    match MakeBufferSize(4)
    | let e: BufferSize => conn.buffer_until(e)
    end

  fun ref on_received(conn: notifier.ServerTCPConnection ref,
    data: Array[U8] iso): ReadAction
  =>
    _out.print(consume data)
    conn.write("Pong")
    KeepReading
