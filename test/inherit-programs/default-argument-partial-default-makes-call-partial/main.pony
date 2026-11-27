// P.f's default argument calls P.g()?, which raises. A call that uses the
// default must be partial, so the try catches it; expects caught.
primitive P
  fun g(): U8 ? => error
  fun f(x: U8 = P.g()?): U8 => x

actor Main
  new create(env: Env) =>
    try env.out.print(P.f().string()) else env.out.print("caught") end
