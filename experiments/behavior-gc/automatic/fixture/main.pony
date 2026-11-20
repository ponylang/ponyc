use @printf[I32](format: Pointer[U8] tag, ...)
use @pony_exitcode[None](code: I32)

class \nodoc\ Holder
  let bytes: Array[U64]

  new create(n: U64) => bytes = Array[U64].init(n, 3)

actor \nodoc\ Probe
  let _seed: U64
  var _outer: (Holder | None) = None
  var _inner: (Holder | None) = None
  var _scratch: Array[U64] = []
  var _sum: U64 = 0

  new create(n: U64) =>
    _seed = n
    measure()

  be measure() =>
    let outer = Holder(_seed)
    middle()
    _outer = outer
    verify()

  fun \noinline\ ref middle() =>
    let inner = Holder(_seed + 11)
    phase()
    _inner = inner

  fun \noinline\ ref phase() =>
    fill()
    finish()

  fun \noinline\ ref fill() =>
    _scratch = Array[U64].init(_seed, 8192)
    try _sum = _scratch(8191)? end
    _scratch = []

  fun \noinline\ ref finish() => _sum = _sum + 1

  be verify() =>
    try
      let a = (_outer as Holder).bytes
      let b = (_inner as Holder).bytes
      a(0)? = a(2)? + 5
      b(0)? = b(1)? + 7
      @printf("%llu %llu %llu\n".cstring(), a(0)?, b(0)?, _sum)
      if (a(0)? == (_seed + 5)) and (b(0)? == (_seed + 18)) and
        (_sum == (_seed + 1))
      then
        @pony_exitcode(0)
      end
    end

actor \nodoc\ Main
  new create(env: Env) =>
    @pony_exitcode(1)
    try Probe(env.args(1)?.u64()?) end
