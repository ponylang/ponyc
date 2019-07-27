use "net"

actor Main
  new create(env: Env) =>
    let limit = try
      env.args(1)?.usize()?
    else
      1
    end

    try
      let auth = env.root as AmbientAuth
      TCPListener(auth, recover Listener(env, limit) end)
      UDPSocket(auth, recover Pong(env) end)
    else
      env.out.print("unable to use the network")
    end
