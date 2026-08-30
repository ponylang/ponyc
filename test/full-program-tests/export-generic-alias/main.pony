use @call_get[I64](b: MyBox[I64] val)
use @call_add[I64](b: MyBox[I64] val, y: I64)

class MyBox[A: (Real[A] val & Integer[A] val)]
  let _value: A

  new val create(value: A) => _value = value
  fun val get(): A => _value
  fun val add(y: A): A => _value + y

type \c_api\ BoxedI64 is MyBox[I64]

actor Main
  new create(env: Env) =>
    let b = MyBox[I64](10)
    let r1 = @call_get(b)
    let r2 = @call_add(b, 5)

    if (r1 == 10) and (r2 == 15) then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
