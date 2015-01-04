use "net"

class ServerSide is TCPConnectionNotify
  let _env: Env

  new create(env: Env) =>
    _env = env

  fun ref accepted(conn: TCPConnection ref) =>
    try
      let (host, service) = conn.remote_address().name()
      _env.out.print("accepted from " + host + ":" + service)
      conn.write("server says hi")
    end

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso) =>
    _env.out.print(consume data)
    conn.dispose()

  fun ref closed(conn: TCPConnection ref) =>
    _env.out.print("server closed")
