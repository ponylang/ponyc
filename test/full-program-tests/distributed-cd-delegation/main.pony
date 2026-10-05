actor Ping
  let _pong: Pong

  new create(pong: Pong) =>
    _pong = pong
    pong.set_ping(this)

actor Pong
  var _ping: (Ping | None) = None
  var _count: U32

  new create(count: U32) =>
    _count = count

  be set_ping(ping: Ping) =>
    _ping = ping
    do_work()

  be do_work() =>
    if _count > 0 then
      _count = _count - 1
      do_work()
    end

actor Main
  new create(env: Env) =>
    let pong = Pong(100)
    let ping = Ping(pong)
    None
