use @printf[I32](fmt: Pointer[U8] tag, ...)
use @pony_get_exitcode[I32]()
use @pony_exitcode[None](code: I32)
use "time"

actor Ring
  let _id: U32
  var _next: (Ring | None)

  new create(id: U32, neighbor: (Ring | None) = None) =>
    _id = id
    _next = neighbor

  be set(neighbor: Ring) =>
    _next = neighbor

  be pass(i: USize) =>
    if i > 0 then
      match _next
      | let n: Ring =>
        n.pass(i - 1)
      end
    end

  fun _final() =>
    let i = @pony_get_exitcode()
    @pony_exitcode(i + 1)

actor Watcher
  let _num: I32
  var _c: I32 = 0

  let _timers: Timers = Timers
  let _timer: Timer tag

  new create(num: I32) =>
    _num = num

    let timer = Timer(WatcherWaker(this), 500_000_000, 500_000_000)
    _timer = timer
    _timers(consume timer)

  be check_done() =>
    if @pony_get_exitcode() != _num then
      _c = _c + 1
      if _c > 50 then
        @printf("Ran out of time... exit count is: %d instead of %d\n".cstring(), @pony_get_exitcode(), _num)
        _exit(1)
      end
    else
      _exit(0)
    end

  fun _exit(code: I32) =>
    _timers.cancel(_timer)
    @pony_exitcode(code)

class WatcherWaker is TimerNotify
  let _watcher: Watcher
  new iso create(watcher: Watcher) =>
    _watcher = watcher

  fun ref apply(timer: Timer, count: U64): Bool =>
    _watcher.check_done()
    true

actor Main
  var _ring_size: U32 = 10
  var _ring_count: U32 = 10
  var _pass: USize = 10

  new create(env: Env) =>
    setup_ring()
    Watcher((_ring_size * _ring_count).i32())

  fun setup_ring() =>
    var j: U32 = 0
    while true do
      let first = Ring(1)
      var next = first

      var k: U32 = 0
      while true do
        let current = Ring(_ring_size - k, next)
        next = current

        k = k + 1
        if k >= (_ring_size - 1) then
          break
        end
      end

      first.set(next)

      if _pass > 0 then
        first.pass(_pass)
      end

      j = j + 1
      if j >= _ring_count then
        break
      end
    end
