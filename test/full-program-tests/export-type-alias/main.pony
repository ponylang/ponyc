use @call_get[I64](w: Wrapper val)

class val Wrapper
  let _x: I64

  new val create(x: I64) =>
    _x = x

  fun val get(): I64 =>
    _x

type \c_api\ Wrapped is Wrapper

actor Main
  new create(env: Env) =>
    let w: Wrapper val = Wrapper(42)
    let result = @call_get(w)

    if result == 42 then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
