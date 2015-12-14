use "net"

actor Main
  new create(env: Env) =>
    let listener = TCPListener.ip4(Listener(env))

class Listener is TCPListenNotify
  let _env: Env
  var _host: String = ""
  var _service: String = ""

  new iso create(env: Env) =>
    _env = env

  fun ref listening(listen: TCPListener ref) =>
    try
      (_host, _service) = listen.local_address().name()
      _env.out.print("listening on " + _host + ":" + _service)
    else
      _env.out.print("couldn't get local address")
      listen.close()
    end

  fun ref not_listening(listen: TCPListener ref) =>
    _env.out.print("couldn't listen")
    listen.close()

  fun ref connected(listen: TCPListener ref) : TCPConnectionNotify iso^ =>
    Server(_env)

class Server is TCPConnectionNotify
  let _env: Env

  new iso create(env: Env) =>
    _env = env

  fun ref accepted(conn: TCPConnection ref) =>
    _env.out.print("connection accepted")

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso) =>
    _env.out.print("data received, looping it back")
    conn.write("server says: ")
    conn.write(consume data)

  fun ref closed(conn: TCPConnection ref) =>
    _env.out.print("server closed")
