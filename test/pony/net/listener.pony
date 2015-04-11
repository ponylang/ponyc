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
            SSLConnection(
              ClientSide(env),
              SSL(
                SSLContext
                  .set_ca_file("./test/pony/net/cert.pem")
                  .set_verify(true),
                false
                )
              )
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
        SSLConnection(
          ServerSide(env),
          SSL(
            SSLContext
              .set_cert("./test/pony/net/cert.pem", "./test/pony/net/key.pem")
              .set_verify(false),
            true
            )
          )
      end
    else
      _env.out.print("couldn't create server side")
      error
    end
