actor Ping
  var _pong: (Pong | None)

  new create(pong: Pong) =>
    _pong = pong
    pong.set_ping(this)

  be drop_pong() =>
    _pong = None

actor Pong
  var _ping: (Ping | None) = None

  new create() =>
    None

  be set_ping(ping: Ping) =>
    _ping = ping

actor Main
  new create(env: Env) =>
    let pong = Pong
    let ping = Ping(pong)
    ping.drop_pong()
