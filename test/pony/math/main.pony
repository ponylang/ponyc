use "math"

actor Main
  var _env: Env

  new create(env: Env) =>
    _env = env

    var x: U32 = 7
    var y: U32 = x
    _env.out.print(y.string())

    test_i128_f()
    test_u128_f()
    test_fib(10)

  fun ref test_i128_f() =>
    var x: I128 = 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
    var y: I128 = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF

    _env.out.print(x.string() + ", " + y.string())

    var xf = x.f64()
    var yf = y.f64()

    _env.out.print(xf.string() + ", " + yf.string())

    x = xf.i128()
    y = yf.i128()

    _env.out.print(x.string() + ", " + y.string())

  fun ref test_u128_f() =>
    var x: U128 = 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
    var y: U128 = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF

    _env.out.print(x.string() + ", " + y.string())

    var xf = x.f64()
    var yf = y.f64()

    _env.out.print(xf.string() + ", " + yf.string())

    x = xf.u128()
    y = yf.u128()

    _env.out.print(x.string() + ", " + y.string())

  fun ref test_fib(n: U64) =>
    _env.out.print("=== Fibonacci Generator ===")

    var i: U8 = 0

    for fib in Fibonacci[U128]() do
      _env.out.print("Fibonacci(" + i.string() + ")= " + fib.string())
      i = i + 1
      if i == n then break fib end
    end

    _env.out.print("=== Fibonacci Application ===")

    var fib = Fibonacci[U128]()

    for j in Range[U8](0, n) do
      _env.out.print("Fibonacci(" + j.string() + ")= " + fib(j).string())
    end
