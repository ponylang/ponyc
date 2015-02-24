use "net"

class ClientSide is TCPConnectionNotify
  let _env: Env

  new create(env: Env) =>
    _env = env

  fun ref connecting(conn: TCPConnection ref, count: U32) =>
    _env.out.print("connecting: " + count.string())

  fun ref connected(conn: TCPConnection ref) =>
    try
      (let host, let service) = conn.remote_address().name()
      _env.out.print("connected to " + host + ":" + service)
      conn.set_nodelay(true)
      conn.set_keepalive(10)
      conn.write("client says hi")
    end

  fun ref connect_failed(conn: TCPConnection ref) =>
    _env.out.print("connect failed")

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso) =>
    _env.out.print(consume data)

  fun ref closed(conn: TCPConnection ref) =>
    _env.out.print("client closed")
