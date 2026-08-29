use @call_add[I64](x: I64, y: I64)
use @call_noop[None]()
use @call_inc[I64](x: I64)

primitive \c_api\ Math
  fun val add(x: I64, y: I64): I64 => x + y
  fun val noop(): None => None
  fun val inc(x': I64): I64 => x' + 1

actor Main
  new create(env: Env) =>
    let result = @call_add(17, 25)
    @call_noop()
    let inc_result = @call_inc(9)

    if (result == 42) and (inc_result == 10) then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
