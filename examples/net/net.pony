use "net"

actor Main
  new create(env: Env) =>
    let limit = try
      env.args(1)?.usize()?
    else
      1
    end

    TCPListener(env.root, recover Listener(env, limit) end)
    UDPSocket(env.root, recover Pong(env) end)
