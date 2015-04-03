class Test
  var i: U32 = 3

  fun ref adder(j: U32): U32 =>
    i + j

actor Main
  new create(env: Env) =>
    var test = Test
    test.adder(4)
