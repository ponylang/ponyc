class Helper[A: Arithmetic]
  /*var x: (A, I64)
  var y: (Main | None)*/
  var x: A
  var y: Main

  new create(x': A, y': Main) =>
    /*x = (x', 7)*/
    x = x'
    y = y'

actor Main
  var x: U32
  var y: Helper[F16]
  var z: Bool

  new create(env: Env) =>
    x = 7
    y = Helper[F16](9, this)
    z = True
