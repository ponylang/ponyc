actor Main
  new create(env: Env) =>
    let a: U64 = 3
    let b: U64 = 2
    let result = a + b
    if result == U64(5) then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
