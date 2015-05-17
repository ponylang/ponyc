use "net"
use "net/ssl"

actor Main
  new create(env: Env) =>
    let ssl = try
      if env.args(1) == "ssl" then
        true
      else
        false
      end
    else
      false
    end

    TCPListener(recover Listener(env, ssl) end)
    UDPSocket(recover Pong(env) end)
