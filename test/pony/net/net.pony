use "net"
use "net/ssl"

actor Main
  new create(env: Env) =>
    let ssl = try
      if env.args(1) == "ssl" then
        SSLInit
        true
      else
        false
      end
    else
      false
    end

    TCPListener(recover Listener(env, ssl) end)
    // UDPSocket.ip4(recover Pong(env) end)
