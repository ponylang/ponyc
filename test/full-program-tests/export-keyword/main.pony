use @call_bar[I64](foo: Foo)
use @call_pass_through[I64](foo: Foo, other: Foo)

class \c_api\ val Foo
  let _x: I64

  new val create(x: I64) =>
    _x = x

  fun val bar(): I64 =>
    _x * 2

  fun val add_other(other: Foo): I64 =>
    _x + other.bar()

actor Main
  new create(env: Env) =>
    let foo: Foo val = Foo(21)
    let result = @call_bar(foo)
    let other: Foo val = Foo(10)
    let pass_result = @call_pass_through(foo, other)

    if (result == 42) and (pass_result == 41) then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
