use math = "mathlib"
use @add_from_c[I64](adder: math.Adder, x: I64)

actor Main
  new create(env: Env) =>
    let a: math.Adder val = math.Adder(32)
    let result = @add_from_c(a, 10)

    if result == 42 then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
