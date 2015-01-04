use "net"

actor Main
  new create(env: Env) =>
    TCPListener(recover Listener(env) end)
    UDPSocket(recover Pong(env) end)
