// C code hands Pony a struct in memory from the runtime's pool allocator. The
// struct's type is embedded in an actor elsewhere, but this memory is not an
// actor, and the GC must not treat it as one.
use @ponyint_pool_alloc_size[NullablePointer[S]](size: USize)
use @memset[Pointer[None]](p: NullablePointer[S] tag, c: I32, n: USize)

class D
  let n: U64

  new create(n': U64) =>
    n = n'

struct S
  var x: U64 = 0
  var d: D = D(1)

  new create() =>
    None

actor Holder
  embed s: S = S

  be ping() =>
    None

actor Main
  let _env: Env
  var _s: NullablePointer[S] = NullablePointer[S].none()
  let _h: Holder = Holder
  var _i: USize = 0
  var _last: D = D(0)

  new create(env: Env) =>
    _env = env
    let np = @ponyint_pool_alloc_size(USize(64))
    @memset(np, 0, 64)
    try
      np()?.d = D(42)
      _s = np
    end
    churn()

  be churn() =>
    var k: USize = 0
    while k < 1000 do
      _last = D(k.u64())
      k = k + 1
    end
    try
      if _s()?.d.n != 42 then _env.exitcode(1) end
    else
      _env.exitcode(1)
    end
    _i = _i + 1
    if _i < 50 then churn() end
