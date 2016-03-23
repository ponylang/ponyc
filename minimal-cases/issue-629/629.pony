"""
Prior to fix for issue 629, this would eventually cause a segfault
due to SSLfree being called on a buffer we had already freed via
dispose in ssl.pony
"""
use "net"
use "net/ssl"

actor Main
  new create(env:Env) =>
    Tester(env)

actor Tester
  var _conn:(TCPConnection | None) = None
  let _env:Env

  new create(env:Env) =>
    _env = env
    connect()

  be connect() =>
    try
      let ctx = SSLContext.set_client_verify(false)
      let ssl = ctx.client()

      _conn = TCPConnection(SSLConnection(ResponseBuilder(_env, this), consume ssl),     "127.0.0.1", "8443".string())
      (_conn as TCPConnection).write("GET /index.html HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n")
    else
      _env.out.print("failed to connect")
    end

  be reconnect() =>
    try
      (_conn as TCPConnection).dispose()
    end
      connect()

class ResponseBuilder is TCPConnectionNotify
  let _buffer:Buffer = Buffer
  let _env:Env
  let _tester:Tester

  new iso create(env:Env, tester:Tester) =>
    _env = env
    _tester = tester

  fun ref auth_failed(conn: TCPConnection ref) =>
    //_env.out.print("auth_failed")
    None

  fun ref received(conn: TCPConnection ref, data: Array[U8] iso) =>
    _buffer.append(consume data)
    try
      while true do
         let s = _buffer.line()
         //_env.out.print(s)
         if s.at("</html", 0) then
           _tester.reconnect()
         end
      end
    end

  fun ref connecting(conn: TCPConnection ref, count: U32) =>
    //_env.out.print("connecting")
    None
