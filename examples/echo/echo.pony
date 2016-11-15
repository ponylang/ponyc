use "net"

actor Main
  new create(env: Env) =>
    try
      TCPListener(env.root as AmbientAuth, Listener(env.out))
    else
      env.out.print("unable to use the network")
    end

class Listener is TCPListenNotify
  let _out: OutStream
  var _host: String = ""
  var _port: String = ""

  new iso create(out: OutStream) =>
    _out = out

  fun ref listening(listen: TCPListener ref) =>
    try
      (_host, _port) = listen.local_address().name()
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
  let _out: OutStream

  new iso create(out: OutStream) =>
    _out = out

  fun ref accepted(conn: TCPConnection ref) =>
    _out.print("connection accepted")

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso): Bool =>
    _out.print("data received, looping it back")
    conn.write("server says: ")
    conn.write(consume data)
    true

  fun ref closed(conn: TCPConnection ref) =>
    _out.print("server closed")

  fun ref connect_failed(conn: TCPConnection ref) =>
    _out.print("connect failed")
