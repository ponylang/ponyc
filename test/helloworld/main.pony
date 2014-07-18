class Helper[A: Arithmetic]
  var x: A
  var y: Main

  new create(x': A, y': Main) =>
    x = x'
    y = y'

actor Main
  var x: U32
  var y: Helper[I16]

  new create(args: Array[String] val) =>
    x = 7
    y = Helper[I16](9, this)
