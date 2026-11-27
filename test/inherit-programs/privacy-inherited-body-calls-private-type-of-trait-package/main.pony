// main's Bar inherits describe(), which calls name() on lib's private
// _Helper. Expects "lib._Helper": the body is written in lib, so the private
// type is allowed even though the class that inherits it is in main.
use l = "lib"
class Bar is l.Foo
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(Bar.describe())
