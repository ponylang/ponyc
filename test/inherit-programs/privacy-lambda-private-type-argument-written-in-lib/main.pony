// lib's FooH fills Foo with lib's private _H, and f() calls name() inside a
// lambda. Expects "lib._H": the lambda is written in lib, so the call on the
// private type is allowed when main's Bar inherits f.
use l = "lib"
class Bar is l.FooH
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(Bar.f(l.MakeH()))
