actor Main
  new create(env: Env) =>
    let a: U64 = 1
    let b: U64 = 2
    let c = a + b
    env.out.print("baseline-fail-marker")
    env.err.print("baseline-fail-stderr-marker")
    env.exitcode(1)
