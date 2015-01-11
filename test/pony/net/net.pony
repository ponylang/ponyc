use "net"

actor Main
  new create(env: Env) =>
    TCPListener(recover Listener(env) end)
    UDPSocket.ip4(recover Pong(env) end)
