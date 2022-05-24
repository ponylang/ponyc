actor Main
  new create(env: Env) =>
    Bar.take_foo(Foo(88))
    let bar: Foo iso = Foo(88)

class Foo
  new create(v: U8) =>
    None

primitive Bar
  fun take_foo(foo: Foo iso) =>
    None
