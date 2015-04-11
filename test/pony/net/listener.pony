use "net"
use "net/ssl"

class Listener is TCPListenNotify
  let _env: Env

  new create(env: Env) =>
    _env = env

  fun ref listening(listen: TCPListener ref) =>
    try
      (let host, let service) = listen.local_address().name()
      _env.out.print("listening on " + host + ":" + service)

      try
        let env = _env

        TCPConnection(
          recover
            let ctx = SSLContext
              .set_ca_file("./test/pony/net/cert.pem")
              .set_verify(true)

            let ssl = SSLConnection(ClientSide(env), SSL(ctx, false))
            ctx.dispose()
            consume ssl
          end,
          host,
          service
          )
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
    listen.close()

    try
      recover
        let ctx = SSLContext
          .set_cert("./test/pony/net/cert.pem", "./test/pony/net/key.pem")
          .set_verify(false)

        let ssl = SSLConnection(ServerSide(env), SSL(ctx, true))
        ctx.dispose()
        consume ssl
      end
    else
      _env.out.print("couldn't create server side")
      error
    end
