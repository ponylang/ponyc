actor Main
  new create(env: Env) =>
    let a: U64 = 3
    let b: U64 = 2
    if (a + b) == U64(5) then
      env.out.print("baseline-marker")
      env.err.print("stderr-marker")
      env.exitcode(1)
    end
