actor Ping
  let _pong: Pong

  new create(pong: Pong) =>
    _pong = pong
    pong.set_ping(this)

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
    None
