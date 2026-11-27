// lib's Foo.make() has `h: Helper = Helper`, and main declares its own Helper.
// The test expects "lib.Helper": the default argument resolves in lib, where
// it is written.
use l = "lib"

actor Main
  new create(env: Env) =>
    env.out.print(Bar.make())

primitive Helper
  fun name(): String => "main.Helper (wrong)"

class Bar is l.Foo
