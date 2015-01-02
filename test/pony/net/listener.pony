use "net"

class Listener is TCPListenNotify
  let _env: Env

  new create(env: Env) =>
    _env = env

  fun ref listening(listen: TCPListener ref) =>
    try
      let (host, service) = listen.local_address().name()
      _env.out.print("listening on " + host + ":" + service)

      let env = _env
      TCPConnection(recover ClientSide(env) end, host, service)
    else
      _env.out.print("couldn't get local address")
      listen.dispose()
    end

  fun ref not_listening(listen: TCPListener ref) =>
    _env.out.print("not listening")
    listen.dispose()

  fun ref connected(listen: TCPListener ref): TCPConnectionNotify iso^ =>
    let env = _env
    listen.dispose()
    recover ServerSide(env) end
