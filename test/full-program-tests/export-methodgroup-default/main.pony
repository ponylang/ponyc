use @call_describe[I64](b: MyBox[U32] val)

trait Describable
  fun val label(): String

class MyBox[A: Any val]
  let _value: A

  new val create(value: A) => _value = value

  fun val describe(): I64 => 42

  fun val describe(): I64 iftype A <: Describable val =>
    99

type \c_api\ BoxedU32 is MyBox[U32]

actor Main
  new create(env: Env) =>
    let b = MyBox[U32](10)
    let r = @call_describe(b)

    if r == 42 then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
