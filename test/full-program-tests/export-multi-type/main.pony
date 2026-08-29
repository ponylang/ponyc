use @check_results[I64](a: Adder, m: Multiplier)

class \c_api\ val Adder
  let _x: I64

  new val create(x: I64) =>
    _x = x

  fun val add(y: I64): I64 =>
    _x + y

  fun val add_twice(y: I64): I64 =>
    _x + y + y

  fun val identity[T: Any val](t: T): T =>
    t

class \c_api\ val Multiplier
  let _x: I64

  new val create(x: I64) =>
    _x = x

  fun val mul(y: I64): I64 =>
    _x * y

  fun val mul_add(y: I64, z: I64): I64 =>
    (_x * y) + z

actor Main
  new create(env: Env) =>
    let a: Adder val = Adder(10)
    let m: Multiplier val = Multiplier(3)
    let result = @check_results(a, m)

    if result == 0 then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
