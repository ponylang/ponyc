struct Foo

class Bar
  let f: Foo = Foo

actor Main
  new create(env: Env) =>
    inspect(recover Bar end)

  be inspect(wrap: Bar iso) =>
    None // Do something with wrap.f
