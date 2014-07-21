class Helper[A: Arithmetic]
  var x: (A, I64)
  var y: (Main | None)

  new create(x': A, y': Main) =>
    x = (x', 7)
    y = y'

actor Main
  var x: U32
  var y: Helper[F16]
  var z: Bool

  new create(env: Env) =>
    x = 7
    y = Helper[F16](9, this)
    z = True
