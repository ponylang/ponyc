// A lambda with no result type returns None, even when its body computes a
// value. Calling it and printing the result must print "None".
actor Main
  new create(env: Env) =>
    let f = {(x: U8) => x + 1}
    env.out.print(f(1).string())
