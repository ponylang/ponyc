use @call_describe[I64](b: MyBox[Stringable val] val)

class MyBox[A: Any val]
  let _value: A

  new val create(value: A) => _value = value

  fun val describe(): I64 => 0

  fun val describe(): I64 iftype A <: Stringable val =>
    _value.string().size().i64()

type \c_api\ BoxedStringable is MyBox[Stringable val]

actor Main
  new create(env: Env) =>
    let b = MyBox[Stringable val]("hello")
    let r = @call_describe(b)

    if r == 5 then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
