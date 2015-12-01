use "net"
use "net/ssl"

actor Main
  new create(env: Env) =>
    var i = USize(1)

    let ssl = try
      if env.args(i) == "ssl" then
        i = i + 1
        true
      else
        false
      end
    else
      false
    end

    let limit = try
      env.args(i).usize()
    else
      1
    end

    TCPListener(recover Listener(env, ssl, limit) end)
    // UDPSocket(recover Pong(env) end)
