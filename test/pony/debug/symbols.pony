actor Main
  var self: Main
  let int: U64
  var tuple: (U64, U64)
  var union: (Env | None)
  var intersect: (ArithmeticConvertible val & Unsigned)
  var pparray: Array[Main]
  var generic: Array[U64]
  var traitfield: ArithmeticConvertible val
  var generictrait: Ordered[String box] val

  new create(env: Env) =>
    self = this
    int = 1
    tuple = (2, 3)
    union = None
    intersect = U64(0)
    pparray = Array[Main]
    generic = Array[U64]
    traitfield = U64(0)
    generictrait = "Test"

    var x: U64 = 0
    env.out.print("Wahey!")
