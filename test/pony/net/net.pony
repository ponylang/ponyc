use "net"

class ClientSide is TCPConnectionNotify
  let _env: Env

  new create(env: Env) =>
    _env = env

  fun ref connecting(conn: TCPConnection ref) =>
    _env.out.print("connecting")

  fun ref connected(conn: TCPConnection ref) =>
    try
      let (host, service) = conn.remote_address()
      _env.out.print("connected to " + host + ":" + service)
      conn.set_nodelay(true)
      conn.set_keepalive(10)
      conn.write("client says hi")
    end

  fun ref connect_failed(conn: TCPConnection ref) =>
    _env.out.print("connect failed")

  fun ref read(conn: TCPConnection ref, data: Array[U8] iso) =>
    _env.out.print(consume data)

  fun ref closed(conn: TCPConnection ref) =>
    _env.out.print("client closed")

class ServerSide is TCPConnectionNotify
  let _env: Env

  new create(env: Env) =>
    _env = env

  fun ref accepted(conn: TCPConnection ref) =>
    try
      let (host, service) = conn.remote_address()
      _env.out.print("accepted from " + host + ":" + service)
      conn.write("server says hi")
    end

  fun ref read(conn: TCPConnection ref, data: Array[U8] iso) =>
    _env.out.print(consume data)
    conn.dispose()

  fun ref closed(conn: TCPConnection ref) =>
    _env.out.print("server closed")

class Listener is TCPListenNotify
  let _env: Env

  new create(env: Env) =>
    _env = env

  fun ref listening(listen: TCPListener ref) =>
    try
      let (host, service) = listen.local_address()
      _env.out.print("listening on " + host + ":" + service)

      let env = _env
      TCPConnection(recover ClientSide(env) end, host, service)
    else
      not_listening(listen)
    end

  fun ref not_listening(listen: TCPListener ref) =>
    _env.out.print("not listening")
    listen.dispose()

  fun ref connection(listen: TCPListener ref): TCPConnectionNotify iso^ =>
    let env = _env
    listen.dispose()
    recover ServerSide(env) end

actor Main
  new create(env: Env) =>
    TCPListener(recover Listener(env) end)
