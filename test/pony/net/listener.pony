use "net"
use "net/ssl"

class Listener is TCPListenNotify
  let _env: Env
  let _sslctx: (SSLContext | None)

  new create(env: Env, ssl: Bool) =>
    _env = env

    _sslctx = if ssl then
      try
        recover
          SSLContext
            .set_authority("./test/pony/net/cert.pem")
            .set_cert("./test/pony/net/cert.pem", "./test/pony/net/key.pem")
            .set_client_verify(true)
            .set_server_verify(true)
        end
      end
    end

  fun ref listening(listen: TCPListener ref) =>
    try
      (let host, let service) = listen.local_address().name()
      _env.out.print("listening on " + host + ":" + service)

      try
        let env = _env

        match _sslctx
        | let ctx: SSLContext =>
          let ssl = ctx.client()
          TCPConnection(SSLConnection(ClientSide(env), consume ssl), host,
            service)
        else
          TCPConnection(ClientSide(env), host, service)
        end
      else
        _env.out.print("couldn't create client side")
        listen.close()
      end
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
      match _sslctx
      | let ctx: SSLContext =>
        let ssl = ctx.server()
        SSLConnection(ServerSide(env, listen), consume ssl)
      else
        ServerSide(env, listen)
      end
    else
      _env.out.print("couldn't create server side")
      error
    end
