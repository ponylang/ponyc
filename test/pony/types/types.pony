class Test
  var i: U32 = 3

  fun ref adder(j: U32): U32 =>
    i + j

actor Main
  let _env: Env

  new create(env: Env) =>
    _env = env

    var test = Test
    test.adder(4)
