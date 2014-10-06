actor Main
  var _env: Env

  new create(env: Env) =>
    _env = env

    var x = U32(7)
    var y: U32 = x
    _env.stdout.print(y.string())

    test_i128_f()
    test_u128_f()

  fun ref test_i128_f() =>
    var x: I128 = 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
    var y: I128 = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF

    _env.stdout.print(x.string() + ", " + y.string())

    var xf = x.f64()
    var yf = y.f64()

    _env.stdout.print(xf.string() + ", " + yf.string())

    x = xf.i128()
    y = yf.i128()

    _env.stdout.print(x.string() + ", " + y.string())

  fun ref test_u128_f() =>
    var x: U128 = 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
    var y: U128 = 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF

    _env.stdout.print(x.string() + ", " + y.string())

    var xf = x.f64()
    var yf = y.f64()

    _env.stdout.print(xf.string() + ", " + yf.string())

    x = xf.u128()
    y = yf.u128()

    _env.stdout.print(x.string() + ", " + y.string())
