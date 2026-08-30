use @call_public[I64](f: Filtered)

class \c_api\ val Filtered
  let _x: I64

  new val create(x: I64) =>
    _x = x

  fun val public_method(): I64 =>
    _x * 2

  fun val _private_method(): I64 =>
    _x * 3

  fun val partial_method(): I64 ? =>
    if _x == 0 then error end
    _x * 4

  fun val tuple_method(): (I64, I64) =>
    (_x, _x * 5)

  fun val takes_tuple(t: (I64, I64)): I64 =>
    t._1 + t._2

  fun @bare_method(x: I64): I64 =>
    x * 6

actor Main
  new create(env: Env) =>
    let f: Filtered val = Filtered(10)
    let result = @call_public(f)

    if result == 20 then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
