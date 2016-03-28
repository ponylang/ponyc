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

    try
      let auth = env.root as AmbientAuth
      TCPListener(auth, recover Listener(env, ssl, limit) end)
      UDPSocket(auth, recover Pong(env) end)
    else
      env.out.print("unable to use the network")
    end
