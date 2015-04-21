use "net"

class ServerSide is TCPConnectionNotify
  let _env: Env
  let _listen: TCPListener

  new iso create(env: Env, listen: TCPListener) =>
    _env = env
    _listen = listen

  fun ref accepted(conn: TCPConnection ref) =>
    try
      (let host, let service) = conn.remote_address().name()
      _env.out.print("accepted from " + host + ":" + service)
      conn.write("server says hi")
    end

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso) =>
    _env.out.print(consume data)
    conn.dispose()
    _listen.dispose()

  fun ref closed(conn: TCPConnection ref) =>
    _env.out.print("server closed")
