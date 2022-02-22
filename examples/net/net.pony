use "net"

actor Main
  new create(env: Env) =>
    let limit = try
      env.args(1)?.usize()?
    else
      1
    end

    TCPListener(TCPListenAuth(env.root), recover Listener(env, limit) end)
    UDPSocket(UDPAuth(env.root), recover Pong(env) end)
