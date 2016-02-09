"""
Simple example of creating and calling a lambda function.
"""

actor Main
  new create(env: Env) =>
    // First let's pass an add function.
    let x = f(lambda(a: U32, b: U32): U32 => a + b end, 6)
    env.out.print("Add: " + x.string())

    // Now a multiply.
    let y = f(lambda(a: U32, b: U32): U32 => a * b end, 6)
    env.out.print("Mult: " + y.string())

    // And finally a lambda that raises an error.
    let z = f(lambda(a: U32, b: U32): U32 ? => error end, 6)
    env.out.print("Error: " + z.string())

  fun f(fn: {(U32, U32): U32 ?} val, x: U32): U32 =>
    """
    We take a function that combines two U32s into one via some operation, and
    a U32. We take the U32 we are given, and one of our choosing, and feed them
    both into the given function.
    """
    try
      fn(x, 3)
    else
      // Lambda function raised an error, just return 0.
      0
    end
