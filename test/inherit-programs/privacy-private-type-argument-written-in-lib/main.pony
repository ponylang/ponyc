// lib's FooH fills Foo with lib's private _H, and main's Bar inherits f(),
// which calls name() on it. Expects "lib._H": the type argument is written in
// lib, so the private type is allowed in the inherited body.
use l = "lib"
class Bar is l.FooH
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(Bar.f(l.MakeH()))
