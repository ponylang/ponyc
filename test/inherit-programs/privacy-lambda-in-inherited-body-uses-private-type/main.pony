// A lambda in lib's trait method calls name() on lib's private _Helper, and
// main's Bar inherits the method. Expects "lib._Helper": the lambda is written
// in lib, so lib's private type is allowed inside it.
use l = "lib"

actor Main
  new create(env: Env) =>
    env.out.print(Bar.make())

class Bar is l.Foo
