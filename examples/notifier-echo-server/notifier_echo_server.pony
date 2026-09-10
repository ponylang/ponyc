"""
Minimal echo server using the notifier API.

Shows the same echo-server behavior as the `echo-server` example, but with
notifier traits instead of the net package's class-and-trait delegation. The
three building blocks here are `TCPListener`, `TCPListenNotify`, and
`ServerTCPConnectionNotify`. There are no user-defined actors — the net
package's concrete actors handle all I/O plumbing.

Connect with any TCP client (e.g. `netcat localhost 7679`) and type to see
your input echoed back.
"""
use "net"
use notifier = "net/notifier"

actor Main
  new create(env: Env) =>
    notifier.TCPListener(
      TCPListenAuth(env.root),
      recover EchoListenNotify(env.out) end,
      "",
      "7679")

class EchoListenNotify is notifier.TCPListenNotify
  """
  Listener callbacks: prints status messages and creates an EchoNotify for
  each accepted connection.
  """
  let _out: OutStream

  new create(out: OutStream) =>
    _out = out

  fun ref on_listening(listen: notifier.TCPListener ref) =>
    _out.print("Echo server started.")

  fun ref on_not_listening(listen: notifier.TCPListener ref) =>
    _out.print("Couldn't start Echo server. " +
      "Perhaps try another network interface?")

  fun ref on_connected(listen: notifier.TCPListener ref):
    notifier.ServerTCPConnectionNotify iso^
  =>
    recover EchoNotify(_out) end

  fun ref on_closed(listen: notifier.TCPListener ref) =>
    _out.print("Echo server shut down.")

class EchoNotify is notifier.ServerTCPConnectionNotify
  """
  Connection callbacks: sends received data back to the client.
  """
  let _out: OutStream

  new create(out: OutStream) =>
    _out = out

  fun ref on_start_failure(conn: notifier.ServerTCPConnection ref,
    reason: StartFailureReason)
  =>
    None

  fun ref on_received(conn: notifier.ServerTCPConnection ref,
    data: Array[U8] iso): ReadAction
  =>
    _out.print("Data received. Echoing it back.")
    conn.write(consume data)
    KeepReading

  fun ref on_closed(conn: notifier.ServerTCPConnection ref) =>
    _out.print("Connection Closed")
