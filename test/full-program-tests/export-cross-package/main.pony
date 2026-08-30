use "mylib"
use @add_from_c[I64](adder: Adder, x: I64)

actor Main
  new create(env: Env) =>
    let a: Adder val = Adder(32)
    let result = @add_from_c(a, 10)

    if result == 42 then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
