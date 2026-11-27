// A lambda has no result type, so it returns None, and the program adds 1 to
// its result. The test expects "couldn't find 'add' in 'None'", the language
// rule for lambdas without a result type.
actor Main
  new create(env: Env) =>
    let f = {(x: U32) => x + 1}
    env.out.print((f(2) + 1).string())
