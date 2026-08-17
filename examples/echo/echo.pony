"""
Demonstrates a basic TCP echo server that accepts connections and sends
received data back to the client.
"""
use "net"

class Listener is TCPListenNotify
  """
  Accepts incoming TCP connections and creates a Server
  notifier for each.
  """
  let _out: OutStream
  var _host: String = ""
  var _port: String = ""

  new iso create(out: OutStream) =>
    _out = out

  fun ref listening(listen: TCPListener ref) =>
    try
      (_host, _port) = listen.local_address().name()?
      _out.print("listening on " + _host + ":" + _port)
    else
      _out.print("couldn't get local address")
      listen.close()
    end

  fun ref not_listening(listen: TCPListener ref) =>
    _out.print("couldn't listen")
    listen.close()

  fun ref connected(listen: TCPListener ref): TCPConnectionNotify iso^ =>
    Server(_out)

class Server is TCPConnectionNotify
  """
  Echoes received data back to the sender.
  """
  let _out: OutStream

  new iso create(out: OutStream) =>
    _out = out

  fun ref accepted(conn: TCPConnection ref) =>
    _out.print("connection accepted")

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] iso,
    times: USize)
    : Bool
  =>
    """
    Sends received data back to the client.
    """
    _out.print("data received, looping it back")
    conn.write("server says: ")
    conn.write(consume data)
    true

  fun ref closed(conn: TCPConnection ref) =>
    _out.print("server closed")

  fun ref connect_failed(conn: TCPConnection ref) =>
    _out.print("connect failed")
