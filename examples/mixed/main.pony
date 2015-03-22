use "collections"
use "options"

actor Worker
  var _env: Env

  new create(env: Env) =>
    _env = env

  be work() =>
    var a: U64 = 86028157
    var b: U64 = 329545133

    var result = factorize(a*b)

    var correct =
      try (result(0) == 86028157) and (result(1) == 329545133) else false end

    /*if (result.size() != 2) or not correct then
      _env.out.print("factorization error")
    else
      _env.out.print("factorization done")
    end*/

  fun ref factorize(bigint: U64) : Array[U64] =>
    var factors = Array[U64]

    if bigint <= 3 then
      factors.push(bigint)
      return factors
    end

    var d: U64 = 2
    var i: U64 = 0
    var n = bigint

    while d < n do
      if (n % d) == 0 then
        i = i + 1
        factors.push(d)
        n = n / d
      else
        d = if d == 2 then 3 else (d + 2) end
      end
    end

    factors.push(d)
    factors

actor Ring
  var _env: (Env | None)
  var _size: U32
  var _pass: U32
  var _repetitions: U32

  var _worker: (Worker | None)
  var _next: Ring

  new create(env: Env, size: U32, pass: U32, repetitions: U32) =>
    _env = env
    _worker = Worker(env)
    _size = size
    _pass = pass
    _repetitions = repetitions

    match _worker
    | var w: Worker => w.work()
    end


    _next = spawn_ring(_size, _pass)

  new neighbor(n: Ring) =>
    _env = None
    _worker = None
    _next = n
    _size = 0
    _pass = 0
    _repetitions = 0

  be pass(i: U32) =>
    if i > 0 then
      _next.pass(i - 1)
    else
      /*match _env
      | var e: Env => e.out.print("message cycle done")
      end*/

      if _repetitions > 0 then
        _repetitions = _repetitions - 1

        match _worker
        | var w: Worker => w.work()
        end

        _next = spawn_ring(_size, _pass)
      end
    end

  fun tag spawn_ring(size: U32, pass': U32) : Ring =>
    var next: Ring = this

    for i in Range[U32](0, size - 1) do
      next = Ring.neighbor(next)
    end

    if pass' > 0 then
      pass(pass' * size)
    end

    next

actor Main
  var _size: U32 = 50
  var _count: U32 = 20
  var _pass: U32 = 10000
  var _repetitions: U32 = 5
  var _env: Env

  new create(env: Env) =>
    _env = env

    arguments()
    start_benchmark()

  fun ref arguments() =>
    let options = Options(_env)

    options
      .add("size", "s", None, I64Argument)
      .add("count", "c", None, I64Argument)
      .add("pass", "p", None, I64Argument)
      .add("repeat", "r", None, I64Argument)

    for option in options do
      match option
      | ("size", var arg: I64) => _size = arg.u32()
      | ("count", var arg: I64) => _count = arg.u32()
      | ("pass", var arg: I64) => _pass = arg.u32()
      | ("repeat", var arg: I64) => _repetitions = arg.u32()
      | ParseError => usage()
      end
    end

  fun ref start_benchmark() =>
    for i in Range[U32](0, _count) do
      Ring(_env, _size, _pass, _repetitions)
    end

  fun ref usage() =>
    _env.out.print(
      """
      mixed OPTIONS
        --size    N   number of actors in each ring"
        --count   N   number of rings"
        --pass    N   number of messages to pass around each ring"
        --repeat  N   number of times to repeat"
      """
      )
