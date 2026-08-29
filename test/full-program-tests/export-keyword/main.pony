use @call_bar[I64](foo: Foo)

export Foo

class val Foo
  let _x: I64

  new val create(x: I64) =>
    _x = x

  fun val bar(): I64 =>
    _x * 2

actor Main
  new create(env: Env) =>
    let foo: Foo val = Foo(21)
    let result = @call_bar(foo)

    if result == 42 then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
