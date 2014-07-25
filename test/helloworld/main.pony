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
    x = (argc xor 0xF) + (7 % -3) + (argc / not 3)
    z = not (x > 2) or (x <= 7) xor (x != 99)
    y = Helper[F16](9, this)
    s = x

  be hello() => z = False
