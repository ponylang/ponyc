class Helper[A: Arithmetic]
  var x: A
  var y: (Main | None)

  new create(x': A, y': Main) =>
    x = x'
    y = y'

actor Main
  var x: I32
  var y: Helper[F16]
  var z: Bool
  var s: Stringable val

  /*new create(env: Env) =>*/
  new create(argc: I32) =>
    x = (7 * -3) + (argc / 3)
    y = Helper[F16](9, this)
    z = True
    s = x

  be hello() => z = False
