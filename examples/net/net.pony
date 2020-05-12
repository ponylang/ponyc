use "net"

actor Main
  new create(env: Env) =>
    let limit = try
      env.args(1)?.usize()?
    else
      1
    end

    let auth = env.root
    TCPListener(auth, recover Listener(env, limit) end)
    UDPSocket(auth, recover Pong(env) end)
