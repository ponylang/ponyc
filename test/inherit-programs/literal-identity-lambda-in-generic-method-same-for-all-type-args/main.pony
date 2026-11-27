// A lambda in a generic method that uses no type parameter is the same type
// for every type argument. Expects is and digestof to show that the lambdas
// from P.f[U8] and P.f[String] are the same object.
primitive P
  fun f[A: Any val](): {(): U8} val => {(): U8 => 1}

actor Main
  new create(env: Env) =>
    let a = P.f[U8]()
    let b = P.f[String]()
    env.out.print((a is b).string())
    env.out.print(((digestof a) == (digestof b)).string())
