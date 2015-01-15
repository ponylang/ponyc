actor Main
  var self: Main
  let int: U64
  var tuple: (U64, U64)
  var ntuple: (Env, Env)
  var union: (Env | None)
  var intersect: (_ArithmeticConvertible val & Unsigned)
  var generic: Array[U64]
  var traitfield: _ArithmeticConvertible val

  new create(env: Env) =>
    self = this
    int = 1
    tuple = (2, 3)
    ntuple = (env, env)
    union = None
    intersect = U64(0)
    generic = Array[U64]
    traitfield = U64(0)

    env.out.print("Wahey!")
