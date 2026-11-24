// Another actor reaches an embedded struct through a val object and acquires
// it from the owner. The owner's next collection must not crash.
use @pony_ctx[Pointer[None]]()
use @pony_triggergc[None](ctx: Pointer[None])

struct S
  var x: U64 = 5
  new create() => None

class O
  embed s: S = S

actor Keeper
  var _s: S val = recover S end
  be take(s: S val) => _s = s

actor Forwarder
  var _o: (O val | None) = None
  be take(o: O val, main: Main) =>
    _o = o
    Keeper.take(o.s)
    main.collect()

actor Main
  var _o: (O val | None) = None
  new create(env: Env) =>
    let o: O val = recover O end
    _o = o
    Forwarder.take(o, this)
  be collect() =>
    @pony_triggergc(@pony_ctx())
    check()
  be check() => None
