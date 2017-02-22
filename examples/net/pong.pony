use "net"

class Pong is UDPNotify
  let _env: Env

  new create(env: Env) =>
    _env = env

  fun ref listening(sock: UDPSocket ref) =>
    try
      let ip = sock.local_address()
      (let host, let service) = ip.name()
      _env.out.print("Pong: listening on " + host + ":" + service)

      let env = _env
      let auth = env.root as AmbientAuth

      if ip.ip4() then
        UDPSocket.ip4(auth, recover Ping(env, ip) end)
      elseif ip.ip6() then
        UDPSocket.ip6(auth, recover Ping(env, ip) end)
      else
        error
      end
    else
      _env.out.print("Pong: couldn't get local name")
      sock.dispose()
    end

  fun ref not_listening(sock: UDPSocket ref) =>
    _env.out.print("Pong: not listening")
    sock.dispose()

  fun ref received(sock: UDPSocket ref, data: Array[U8] iso, from: NetAddress)
  =>
    try
      (let host, let service) = from.name()
      _env.out.print("from " + host + ":" + service)
    end

    _env.out.print(consume data)
    sock.write("pong!", from)
    sock.dispose()

  fun ref closed(sock: UDPSocket ref) =>
    _env.out.print("Pong: closed")
