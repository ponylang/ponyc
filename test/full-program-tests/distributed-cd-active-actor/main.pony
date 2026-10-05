actor Ping
  let _pong: Pong
  var _count: U32

  new create(pong: Pong, count: U32) =>
    _pong = pong
    pong.set_ping(this)
    _count = count
    do_work()

  be do_work() =>
    if _count > 0 then
      _count = _count - 1
      do_work()
    end

actor Pong
  var _ping: (Ping | None) = None

  new create() =>
    None

  be set_ping(ping: Ping) =>
    _ping = ping

actor Main
  new create(env: Env) =>
    let pong = Pong
    let ping = Ping(pong, 100)
    None
