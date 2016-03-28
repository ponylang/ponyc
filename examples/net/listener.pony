use "net"
use "net/ssl"
use "files"

class Listener is TCPListenNotify
  let _env: Env
  let _limit: USize
  let _sslctx: (SSLContext | None)
  var _host: String = ""
  var _service: String = ""
  var _count: USize = 0

  new create(env: Env, ssl: Bool, limit: USize) =>
    _env = env
    _limit = limit

    _sslctx = if ssl then
      try
        let auth = env.root as AmbientAuth
        recover
          SSLContext
            .set_authority(FilePath(auth, "./test/pony/net/cert.pem"))
            .set_cert(
              FilePath(auth, "./test/pony/net/cert.pem"),
              FilePath(auth, "./test/pony/net/key.pem"))
            .set_client_verify(true)
            .set_server_verify(true)
        end
      end
    end

  fun ref listening(listen: TCPListener ref) =>
    try
      (_host, _service) = listen.local_address().name()
      _env.out.print("listening on " + _host + ":" + _service)
      _spawn(listen)
    else
      _env.out.print("couldn't get local address")
      listen.close()
    end

  fun ref not_listening(listen: TCPListener ref) =>
    _env.out.print("not listening")
    listen.close()

  fun ref connected(listen: TCPListener ref): TCPConnectionNotify iso^ ? =>
    let env = _env

    try
      let server = match _sslctx
      | let ctx: SSLContext =>
        let ssl = ctx.server()
        SSLConnection(ServerSide(env), consume ssl)
      else
        ServerSide(env)
      end

      _spawn(listen)
      server
    else
      _env.out.print("couldn't create server side")
      error
    end

  fun ref _spawn(listen: TCPListener ref) =>
    if (_limit > 0) and (_count >= _limit) then
      listen.dispose()
      return
    end

    _count = _count + 1
    _env.out.print("spawn " + _count.string())

    try
      let env = _env

      match _sslctx
      | let ctx: SSLContext =>
        let ssl = ctx.client()
        TCPConnection(_env.root as AmbientAuth,
          SSLConnection(ClientSide(env), consume ssl), _host, _service)
      else
        TCPConnection(_env.root as AmbientAuth,
          ClientSide(env), _host, _service)
      end
    else
      _env.out.print("couldn't create client side")
      listen.close()
    end
