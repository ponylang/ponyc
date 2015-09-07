use "files"
use "net"
use "net/ssl"

actor Main
  new create(env: Env) =>
    var i = U64(1)

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
      env.args(i).u64()
    else
      1
    end

    try
      TCPListener(recover Listener(env, ssl, limit) end)
    end
    // UDPSocket(recover Pong(env) end)
